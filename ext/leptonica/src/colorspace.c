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
 * \file colorspace.c
 * <pre>
 *
 *      Colorspace conversion between RGB and HSV
 *           PIX        *pixConvertRGBToHSV()
 *           PIX        *pixConvertHSVToRGB()
 *           l_int32     convertRGBToHSV()
 *           l_int32     convertHSVToRGB()
 *           l_int32     pixcmapConvertRGBToHSV()
 *           l_int32     pixcmapConvertHSVToRGB()
 *           PIX        *pixConvertRGBToHue()
 *           PIX        *pixConvertRGBToSaturation()
 *           PIX        *pixConvertRGBToValue()
 *
 *      Selection and display of range of colors in HSV space
 *           PIX        *pixMakeRangeMaskHS()
 *           PIX        *pixMakeRangeMaskHV()
 *           PIX        *pixMakeRangeMaskSV()
 *           PIX        *pixMakeHistoHS()
 *           PIX        *pixMakeHistoHV()
 *           PIX        *pixMakeHistoSV()
 *           PIX        *pixFindHistoPeaksHSV()
 *           PIX        *displayHSVColorRange()
 *
 *      Colorspace conversion between RGB and YUV
 *           PIX        *pixConvertRGBToYUV()
 *           PIX        *pixConvertYUVToRGB()
 *           l_int32     convertRGBToYUV()
 *           l_int32     convertYUVToRGB()
 *           l_int32     pixcmapConvertRGBToYUV()
 *           l_int32     pixcmapConvertYUVToRGB()
 *
 *      Colorspace conversion between RGB and XYZ
 *           FPIXA      *pixConvertRGBToXYZ()
 *           PIX        *fpixaConvertXYZToRGB()
 *           l_int32     convertRGBToXYZ()
 *           l_int32     convertXYZToRGB()
 *
 *      Colorspace conversion between XYZ and LAB
 *           FPIXA      *fpixaConvertXYZToLAB()
 *           PIX        *fpixaConvertLABToXYZ()
 *           l_int32     convertXYZToLAB()
 *           l_int32     convertLABToXYZ()
 *           static l_float32  lab_forward()
 *           static l_float32  lab_reverse()
 *
 *      Colorspace conversion between RGB and LAB
 *           FPIXA      *pixConvertRGBToLAB()
 *           PIX        *fpixaConvertLABToRGB()
 *           l_int32     convertRGBToLAB()
 *           l_int32     convertLABToRGB()
 *
 *      Gamut display of RGB color space
 *           PIX        *pixMakeGamutRGB()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include <math.h>
#include "allheaders.h"

#ifndef  NO_CONSOLE_IO
#define  DEBUG_HISTO       0
#define  SLOW_CUBE_ROOT    0
#endif  /* ~NO_CONSOLE_IO */

    /* Functions used in xyz <--> lab conversions */
static l_float32 lab_forward(l_float32 v);
static l_float32 lab_reverse(l_float32 v);

/*---------------------------------------------------------------------------*
 *                  Colorspace conversion between RGB and HSB                *
 *---------------------------------------------------------------------------*/
/*!
 * \brief   pixConvertRGBToHSV()
 *
 * \param[in]    pixd    can be NULL; if not NULL, must == pixs
 * \param[in]    pixs
 * \return  pixd always
 *
 * <pre>
 * Notes:
 *      (1) For pixs = pixd, this is in-place; otherwise pixd must be NULL.
 *      (2) The definition of our HSV space is given in convertRGBToHSV().
 *      (3) The h, s and v values are stored in the same places as
 *          the r, g and b values, respectively.  Here, they are explicitly
 *          placed in the 3 MS bytes in the pixel.
 *      (4) Normalizing to 1 and considering the r,g,b components,
 *          a simple way to understand the HSV space is:
 *           ~ v = max(r,g,b)
 *           ~ s = (max - min) / max
 *           ~ h ~ (mid - min) / (max - min)  [apart from signs and constants]
 *      (5) Normalizing to 1, some properties of the HSV space are:
 *           ~ For gray values (r = g = b) along the continuum between
 *             black and white:
 *                s = 0  (becoming undefined as you approach black)
 *                h is undefined everywhere
 *           ~ Where one component is saturated and the others are zero:
 *                v = 1
 *                s = 1
 *                h = 0 (r = max), 1/3 (g = max), 2/3 (b = max)
 *           ~ Where two components are saturated and the other is zero:
 *                v = 1
 *                s = 1
 *                h = 1/2 (if r = 0), 5/6 (if g = 0), 1/6 (if b = 0)
 *      (6) Dividing each component by a constant c > 1 reduces the
 *          brightness v, but leaves the saturation and hue invariant.
 * </pre>
 */
PIX *
pixConvertRGBToHSV(PIX  *pixd,
                   PIX  *pixs)
{
l_int32    w, h, d, wpl, i, j, rval, gval, bval, hval, sval, vval;
l_uint32  *line, *data;
PIXCMAP   *cmap;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, pixd);
    if (pixd && pixd != pixs)
        return (PIX *)ERROR_PTR("pixd defined and not inplace", __func__, pixd);

    d = pixGetDepth(pixs);
    cmap = pixGetColormap(pixs);
    if (!cmap && d != 32)
        return (PIX *)ERROR_PTR("not cmapped or rgb", __func__, pixd);

    if (!pixd)
        pixd = pixCopy(NULL, pixs);

    cmap = pixGetColormap(pixd);
    if (cmap) {   /* just convert the colormap */
        pixcmapConvertRGBToHSV(cmap);
        return pixd;
    }

        /* Convert RGB image */
    pixGetDimensions(pixd, &w, &h, NULL);
    wpl = pixGetWpl(pixd);
    data = pixGetData(pixd);
    for (i = 0; i < h; i++) {
        line = data + i * wpl;
        for (j = 0; j < w; j++) {
            extractRGBValues(line[j], &rval, &gval, &bval);
            convertRGBToHSV(rval, gval, bval, &hval, &sval, &vval);
            line[j] = (hval << 24) | (sval << 16) | (vval << 8);
        }
    }

    return pixd;
}


/*!
 * \brief   pixConvertHSVToRGB()
 *
 * \param[in]    pixd    can be NULL; if not NULL, must == pixs
 * \param[in]    pixs
 * \return  pixd always
 *
 * <pre>
 * Notes:
 *      (1) For pixs = pixd, this is in-place; otherwise pixd must be NULL.
 *      (2) The user takes responsibility for making sure that pixs is
 *          in our HSV space.  The definition of our HSV space is given
 *          in convertRGBToHSV().
 *      (3) The h, s and v values are stored in the same places as
 *          the r, g and b values, respectively.  Here, they are explicitly
 *          placed in the 3 MS bytes in the pixel.
 * </pre>
 */
PIX *
pixConvertHSVToRGB(PIX  *pixd,
                   PIX  *pixs)
{
l_int32    w, h, d, wpl, i, j, rval, gval, bval, hval, sval, vval;
l_uint32   pixel;
l_uint32  *line, *data;
PIXCMAP   *cmap;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, pixd);
    if (pixd && pixd != pixs)
        return (PIX *)ERROR_PTR("pixd defined and not inplace", __func__, pixd);

    d = pixGetDepth(pixs);
    cmap = pixGetColormap(pixs);
    if (!cmap && d != 32)
        return (PIX *)ERROR_PTR("not cmapped or hsv", __func__, pixd);

    if (!pixd)
        pixd = pixCopy(NULL, pixs);

    cmap = pixGetColormap(pixd);
    if (cmap) {   /* just convert the colormap */
        pixcmapConvertHSVToRGB(cmap);
        return pixd;
    }

        /* Convert HSV image */
    pixGetDimensions(pixd, &w, &h, NULL);
    wpl = pixGetWpl(pixd);
    data = pixGetData(pixd);
    for (i = 0; i < h; i++) {
        line = data + i * wpl;
        for (j = 0; j < w; j++) {
            pixel = line[j];
            hval = pixel >> 24;
            sval = (pixel >> 16) & 0xff;
            vval = (pixel >> 8) & 0xff;
            convertHSVToRGB(hval, sval, vval, &rval, &gval, &bval);
            composeRGBPixel(rval, gval, bval, line + j);
        }
    }

    return pixd;
}


/*!
 * \brief   convertRGBToHSV()
 *
 * \param[in]    rval, gval, bval      RGB input
 * \param[out]   phval, psval, pvval   comparable HSV values
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The range of returned values is:
 *            h [0 ... 239]
 *            s [0 ... 255]
 *            v [0 ... 255]
 *      (2) If r = g = b, the pixel is gray (s = 0), and we define h = 0.
 *      (3) h wraps around, so that h = 0 and h = 240 are equivalent
 *          in hue space.
 *      (4) h has the following correspondence to color:
 *            h = 0         magenta
 *            h = 40        red
 *            h = 80        yellow
 *            h = 120       green
 *            h = 160       cyan
 *            h = 200       blue
 * </pre>
 */
l_ok
convertRGBToHSV(l_int32   rval,
                l_int32   gval,
                l_int32   bval,
                l_int32  *phval,
                l_int32  *psval,
                l_int32  *pvval)
{
l_int32    minrg, maxrg, min, max, delta;
l_float32  h;

    if (phval) *phval = 0;
    if (psval) *psval = 0;
    if (pvval) *pvval = 0;
    if (!phval || !psval || !pvval)
        return ERROR_INT("&hval, &sval, &vval not all defined", __func__, 1);

    minrg = L_MIN(rval, gval);
    min = L_MIN(minrg, bval);
    maxrg = L_MAX(rval, gval);
    max = L_MAX(maxrg, bval);
    delta = max - min;

    *pvval = max;
    if (delta == 0) {  /* gray; no chroma */
        *phval = 0;
        *psval = 0;
    } else {
        *psval = (l_int32)(255. * (l_float32)delta / (l_float32)max + 0.5);
        if (rval == max)  /* between magenta and yellow */
            h = (l_float32)(gval - bval) / (l_float32)delta;
        else if (gval == max)  /* between yellow and cyan */
            h = 2. + (l_float32)(bval - rval) / (l_float32)delta;
        else  /* between cyan and magenta */
            h = 4. + (l_float32)(rval - gval) / (l_float32)delta;
        h *= 40.0;
        if (h < 0.0)
            h += 240.0;
        if (h >= 239.5)
            h = 0.0;
        *phval = (l_int32)(h + 0.5);
    }

    return 0;
}


/*!
 * \brief   convertHSVToRGB()
 *
 * \param[in]    hval, sval, vval      HSV input
 * \param[out]   prval, pgval, pbval   comparable RGB values
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) See convertRGBToHSV() for valid input range of HSV values
 *          and their interpretation in color space.
 * </pre>
 */
l_ok
convertHSVToRGB(l_int32   hval,
                l_int32   sval,
                l_int32   vval,
                l_int32  *prval,
                l_int32  *pgval,
                l_int32  *pbval)
{
l_int32   i, x, y, z;
l_float32 h, f, s;

    if (prval) *prval = 0;
    if (pgval) *pgval = 0;
    if (pbval) *pbval = 0;
    if (!prval || !pgval || !pbval)
        return ERROR_INT("&rval, &gval, &bval not all defined", __func__, 1);

    if (sval == 0) {  /* gray */
        *prval = vval;
        *pgval = vval;
        *pbval = vval;
    } else {
        if (hval < 0 || hval > 240)
            return ERROR_INT("invalid hval", __func__, 1);
        if (hval == 240)
            hval = 0;
        h = (l_float32)hval / 40.;
        i = (l_int32)h;
        f = h - i;
        s = (l_float32)sval / 255.;
        x = (l_int32)(vval * (1. - s) + 0.5);
        y = (l_int32)(vval * (1. - s * f) + 0.5);
        z = (l_int32)(vval * (1. - s * (1. - f)) + 0.5);
        switch (i)
        {
        case 0:
            *prval = vval;
            *pgval = z;
            *pbval = x;
            break;
        case 1:
            *prval = y;
            *pgval = vval;
            *pbval = x;
            break;
        case 2:
            *prval = x;
            *pgval = vval;
            *pbval = z;
            break;
        case 3:
            *prval = x;
            *pgval = y;
            *pbval = vval;
            break;
        case 4:
            *prval = z;
            *pgval = x;
            *pbval = vval;
            break;
        case 5:
            *prval = vval;
            *pgval = x;
            *pbval = y;
            break;
        default:  /* none possible */
            return 1;
        }
    }

    return 0;
}


/*!
 * \brief   pixcmapConvertRGBToHSV()
 *
 * \param[in]    cmap
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      ~ in-place transform
 *      ~ See convertRGBToHSV() for def'n of HSV space.
 *      ~ replaces: r --> h, g --> s, b --> v
 * </pre>
 */
l_ok
pixcmapConvertRGBToHSV(PIXCMAP  *cmap)
{
l_int32   i, ncolors, rval, gval, bval, hval, sval, vval;

    if (!cmap)
        return ERROR_INT("cmap not defined", __func__, 1);

    ncolors = pixcmapGetCount(cmap);
    for (i = 0; i < ncolors; i++) {
        pixcmapGetColor(cmap, i, &rval, &gval, &bval);
        convertRGBToHSV(rval, gval, bval, &hval, &sval, &vval);
        pixcmapResetColor(cmap, i, hval, sval, vval);
    }
    return 0;
}


/*!
 * \brief   pixcmapConvertHSVToRGB()
 *
 * \param[in]    cmap
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      ~ in-place transform
 *      ~ See convertRGBToHSV() for def'n of HSV space.
 *      ~ replaces: h --> r, s --> g, v --> b
 * </pre>
 */
l_ok
pixcmapConvertHSVToRGB(PIXCMAP  *cmap)
{
l_int32   i, ncolors, rval, gval, bval, hval, sval, vval;

    if (!cmap)
        return ERROR_INT("cmap not defined", __func__, 1);

    ncolors = pixcmapGetCount(cmap);
    for (i = 0; i < ncolors; i++) {
        pixcmapGetColor(cmap, i, &hval, &sval, &vval);
        convertHSVToRGB(hval, sval, vval, &rval, &gval, &bval);
        pixcmapResetColor(cmap, i, rval, gval, bval);
    }
    return 0;
}


/*!
 * \brief   pixConvertRGBToHue()
 *
 * \param[in]    pixs    32 bpp RGB, or 8 bpp with colormap
 * \return  pixd   8 bpp hue of HSV, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The conversion to HSV hue is in-lined here.
 *      (2) If there is a colormap, it is removed.
 *      (3) If you just want the hue component, this does it
 *          at about 10 Mpixels/sec/GHz, which is about
 *          2x faster than using pixConvertRGBToHSV()
 * </pre>
 */
PIX *
pixConvertRGBToHue(PIX  *pixs)
{
l_int32    w, h, d, wplt, wpld;
l_int32    i, j, rval, gval, bval, hval, minrg, min, maxrg, max, delta;
l_float32  fh;
l_uint32   pixel;
l_uint32  *linet, *lined, *datat, *datad;
PIX       *pixt, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);

    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 32 && !pixGetColormap(pixs))
        return (PIX *)ERROR_PTR("not cmapped or rgb", __func__, NULL);
    pixt = pixRemoveColormap(pixs, REMOVE_CMAP_TO_FULL_COLOR);

        /* Convert RGB image */
    pixd = pixCreate(w, h, 8);
    pixCopyResolution(pixd, pixs);
    wplt = pixGetWpl(pixt);
    datat = pixGetData(pixt);
    wpld = pixGetWpl(pixd);
    datad = pixGetData(pixd);
    for (i = 0; i < h; i++) {
        linet = datat + i * wplt;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            pixel = linet[j];
            extractRGBValues(pixel, &rval, &gval, &bval);
            minrg = L_MIN(rval, gval);
            min = L_MIN(minrg, bval);
            maxrg = L_MAX(rval, gval);
            max = L_MAX(maxrg, bval);
            delta = max - min;
            if (delta == 0) {  /* gray; no chroma */
                hval = 0;
            } else {
                if (rval == max)  /* between magenta and yellow */
                    fh = (l_float32)(gval - bval) / (l_float32)delta;
                else if (gval == max)  /* between yellow and cyan */
                    fh = 2. + (l_float32)(bval - rval) / (l_float32)delta;
                else  /* between cyan and magenta */
                    fh = 4. + (l_float32)(rval - gval) / (l_float32)delta;
                fh *= 40.0;
                if (fh < 0.0)
                    fh += 240.0;
                hval = (l_int32)(fh + 0.5);
            }
            SET_DATA_BYTE(lined, j, hval);
        }
    }
    pixDestroy(&pixt);

    return pixd;
}



/*!
 * \brief   pixConvertRGBToSaturation()
 *
 * \param[in]    pixs   32 bpp RGB, or 8 bpp with colormap
 * \return  pixd   8 bpp sat of HSV, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The conversion to HSV sat is in-lined here.
 *      (2) If there is a colormap, it is removed.
 *      (3) If you just want the saturation component, this does it
 *          at about 12 Mpixels/sec/GHz.
 * </pre>
 */
PIX *
pixConvertRGBToSaturation(PIX  *pixs)
{
l_int32    w, h, d, wplt, wpld;
l_int32    i, j, rval, gval, bval, sval, minrg, min, maxrg, max, delta;
l_uint32   pixel;
l_uint32  *linet, *lined, *datat, *datad;
PIX       *pixt, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);

    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 32 && !pixGetColormap(pixs))
        return (PIX *)ERROR_PTR("not cmapped or rgb", __func__, NULL);
    pixt = pixRemoveColormap(pixs, REMOVE_CMAP_TO_FULL_COLOR);

        /* Convert RGB image */
    pixd = pixCreate(w, h, 8);
    pixCopyResolution(pixd, pixs);
    wplt = pixGetWpl(pixt);
    datat = pixGetData(pixt);
    wpld = pixGetWpl(pixd);
    datad = pixGetData(pixd);
    for (i = 0; i < h; i++) {
        linet = datat + i * wplt;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            pixel = linet[j];
            extractRGBValues(pixel, &rval, &gval, &bval);
            minrg = L_MIN(rval, gval);
            min = L_MIN(minrg, bval);
            maxrg = L_MAX(rval, gval);
            max = L_MAX(maxrg, bval);
            delta = max - min;
            if (delta == 0)  /* gray; no chroma */
                sval = 0;
            else
                sval = (l_int32)(255. *
                                 (l_float32)delta / (l_float32)max + 0.5);
            SET_DATA_BYTE(lined, j, sval);
        }
    }

    pixDestroy(&pixt);
    return pixd;
}


/*!
 * \brief   pixConvertRGBToValue()
 *
 * \param[in]    pixs    32 bpp RGB,or 8 bpp with colormap
 * \return  pixd   8 bpp max component intensity of HSV, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The conversion to HSV sat is in-lined here.
 *      (2) If there is a colormap, it is removed.
 *      (3) If you just want the value component, this does it
 *          at about 35 Mpixels/sec/GHz.
 * </pre>
 */
PIX *
pixConvertRGBToValue(PIX  *pixs)
{
l_int32    w, h, d, wplt, wpld;
l_int32    i, j, rval, gval, bval, maxrg, max;
l_uint32   pixel;
l_uint32  *linet, *lined, *datat, *datad;
PIX       *pixt, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);

    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 32 && !pixGetColormap(pixs))
        return (PIX *)ERROR_PTR("not cmapped or rgb", __func__, NULL);
    pixt = pixRemoveColormap(pixs, REMOVE_CMAP_TO_FULL_COLOR);

        /* Convert RGB image */
    pixd = pixCreate(w, h, 8);
    pixCopyResolution(pixd, pixs);
    wplt = pixGetWpl(pixt);
    datat = pixGetData(pixt);
    wpld = pixGetWpl(pixd);
    datad = pixGetData(pixd);
    for (i = 0; i < h; i++) {
        linet = datat + i * wplt;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            pixel = linet[j];
            extractRGBValues(pixel, &rval, &gval, &bval);
            maxrg = L_MAX(rval, gval);
            max = L_MAX(maxrg, bval);
            SET_DATA_BYTE(lined, j, max);
        }
    }

    pixDestroy(&pixt);
    return pixd;
}


/*---------------------------------------------------------------------------*
 *            Selection and display of range of colors in HSV space          *
 *---------------------------------------------------------------------------*/
/*!
 * \brief   pixMakeRangeMaskHS()
 *
 * \param[in]    pixs         32 bpp rgb
 * \param[in]    huecenter    center value of hue range
 * \param[in]    huehw        half-width of hue range
 * \param[in]    satcenter    center value of saturation range
 * \param[in]    sathw        half-width of saturation range
 * \param[in]    regionflag   L_INCLUDE_REGION, L_EXCLUDE_REGION
 * \return  pixd   1 bpp mask over selected pixels, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The pixels are selected based on the specified ranges of
 *          hue and saturation.  For selection or exclusion, the pixel
 *          HS component values must be within both ranges.  Care must
 *          be taken in finding the hue range because of wrap-around.
 *      (2) Use %regionflag == L_INCLUDE_REGION to take only those
 *          pixels within the rectangular region specified in HS space.
 *          Use %regionflag == L_EXCLUDE_REGION to take all pixels except
 *          those within the rectangular region specified in HS space.
 * </pre>
 */
PIX *
pixMakeRangeMaskHS(PIX     *pixs,
                   l_int32  huecenter,
                   l_int32  huehw,
                   l_int32  satcenter,
                   l_int32  sathw,
                   l_int32  regionflag)
{
l_int32    i, j, w, h, wplt, wpld, hstart, hend, sstart, send, hval, sval;
l_int32   *hlut, *slut;
l_uint32   pixel;
l_uint32  *datat, *datad, *linet, *lined;
PIX       *pixt, *pixd;

    if (!pixs || pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs undefined or not 32 bpp", __func__, NULL);
    if (regionflag != L_INCLUDE_REGION && regionflag != L_EXCLUDE_REGION)
        return (PIX *)ERROR_PTR("invalid regionflag", __func__, NULL);

        /* Set up LUTs for hue and saturation.  These have the value 1
         * within the specified intervals of hue and saturation. */
    hlut = (l_int32 *)LEPT_CALLOC(240, sizeof(l_int32));
    slut = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
    sstart = L_MAX(0, satcenter - sathw);
    send = L_MIN(255, satcenter + sathw);
    for (i = sstart; i <= send; i++)
        slut[i] = 1;
    hstart = (huecenter - huehw + 240) % 240;
    hend = (huecenter + huehw + 240) % 240;
    if (hstart < hend) {
        for (i = hstart; i <= hend; i++)
            hlut[i] = 1;
    } else {  /* wrap */
        for (i = hstart; i < 240; i++)
            hlut[i] = 1;
        for (i = 0; i <= hend; i++)
            hlut[i] = 1;
    }

        /* Generate the mask */
    pixt = pixConvertRGBToHSV(NULL, pixs);
    pixGetDimensions(pixs, &w, &h, NULL);
    pixd = pixCreate(w, h, 1);
    if (regionflag == L_INCLUDE_REGION)
        pixClearAll(pixd);
    else  /* L_EXCLUDE_REGION */
        pixSetAll(pixd);
    datat = pixGetData(pixt);
    datad = pixGetData(pixd);
    wplt = pixGetWpl(pixt);
    wpld = pixGetWpl(pixd);
    for (i = 0; i < h; i++) {
        linet = datat + i * wplt;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            pixel = linet[j];
            hval = (pixel >> L_RED_SHIFT) & 0xff;
            sval = (pixel >> L_GREEN_SHIFT) & 0xff;
            if (hlut[hval] == 1 && slut[sval] == 1) {
                if (regionflag == L_INCLUDE_REGION)
                    SET_DATA_BIT(lined, j);
                else  /* L_EXCLUDE_REGION */
                    CLEAR_DATA_BIT(lined, j);
            }
        }
    }

    LEPT_FREE(hlut);
    LEPT_FREE(slut);
    pixDestroy(&pixt);
    return pixd;
}


/*!
 * \brief   pixMakeRangeMaskHV()
 *
 * \param[in]    pixs         32 bpp rgb
 * \param[in]    huecenter    center value of hue range
 * \param[in]    huehw        half-width of hue range
 * \param[in]    valcenter    center value of max intensity range
 * \param[in]    valhw        half-width of max intensity range
 * \param[in]    regionflag   L_INCLUDE_REGION, L_EXCLUDE_REGION
 * \return  pixd   1 bpp mask over selected pixels, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The pixels are selected based on the specified ranges of
 *          hue and max intensity values.  For selection or exclusion,
 *          the pixel HV component values must be within both ranges.
 *          Care must be taken in finding the hue range because of wrap-around.
 *      (2) Use %regionflag == L_INCLUDE_REGION to take only those
 *          pixels within the rectangular region specified in HV space.
 *          Use %regionflag == L_EXCLUDE_REGION to take all pixels except
 *          those within the rectangular region specified in HV space.
 * </pre>
 */
PIX *
pixMakeRangeMaskHV(PIX     *pixs,
                   l_int32  huecenter,
                   l_int32  huehw,
                   l_int32  valcenter,
                   l_int32  valhw,
                   l_int32  regionflag)
{
l_int32    i, j, w, h, wplt, wpld, hstart, hend, vstart, vend, hval, vval;
l_int32   *hlut, *vlut;
l_uint32   pixel;
l_uint32  *datat, *datad, *linet, *lined;
PIX       *pixt, *pixd;

    if (!pixs || pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs undefined or not 32 bpp", __func__, NULL);
    if (regionflag != L_INCLUDE_REGION && regionflag != L_EXCLUDE_REGION)
        return (PIX *)ERROR_PTR("invalid regionflag", __func__, NULL);

        /* Set up LUTs for hue and maximum intensity (val).  These have
         * the value 1 within the specified intervals of hue and value. */
    hlut = (l_int32 *)LEPT_CALLOC(240, sizeof(l_int32));
    vlut = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
    vstart = L_MAX(0, valcenter - valhw);
    vend = L_MIN(255, valcenter + valhw);
    for (i = vstart; i <= vend; i++)
        vlut[i] = 1;
    hstart = (huecenter - huehw + 240) % 240;
    hend = (huecenter + huehw + 240) % 240;
    if (hstart < hend) {
        for (i = hstart; i <= hend; i++)
            hlut[i] = 1;
    } else {
        for (i = hstart; i < 240; i++)
            hlut[i] = 1;
        for (i = 0; i <= hend; i++)
            hlut[i] = 1;
    }

        /* Generate the mask */
    pixt = pixConvertRGBToHSV(NULL, pixs);
    pixGetDimensions(pixs, &w, &h, NULL);
    pixd = pixCreate(w, h, 1);
    if (regionflag == L_INCLUDE_REGION)
        pixClearAll(pixd);
    else  /* L_EXCLUDE_REGION */
        pixSetAll(pixd);
    datat = pixGetData(pixt);
    datad = pixGetData(pixd);
    wplt = pixGetWpl(pixt);
    wpld = pixGetWpl(pixd);
    for (i = 0; i < h; i++) {
        linet = datat + i * wplt;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            pixel = linet[j];
            hval = (pixel >> L_RED_SHIFT) & 0xff;
            vval = (pixel >> L_BLUE_SHIFT) & 0xff;
            if (hlut[hval] == 1 && vlut[vval] == 1) {
                if (regionflag == L_INCLUDE_REGION)
                    SET_DATA_BIT(lined, j);
                else  /* L_EXCLUDE_REGION */
                    CLEAR_DATA_BIT(lined, j);
            }
        }
    }

    LEPT_FREE(hlut);
    LEPT_FREE(vlut);
    pixDestroy(&pixt);
    return pixd;
}


/*!
 * \brief   pixMakeRangeMaskSV()
 *
 * \param[in]    pixs         32 bpp rgb
 * \param[in]    satcenter    center value of saturation range
 * \param[in]    sathw        half-width of saturation range
 * \param[in]    valcenter    center value of max intensity range
 * \param[in]    valhw        half-width of max intensity range
 * \param[in]    regionflag   L_INCLUDE_REGION, L_EXCLUDE_REGION
 * \return  pixd   1 bpp mask over selected pixels, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The pixels are selected based on the specified ranges of
 *          saturation and max intensity (val).  For selection or
 *          exclusion, the pixel SV component values must be within both ranges.
 *      (2) Use %regionflag == L_INCLUDE_REGION to take only those
 *          pixels within the rectangular region specified in SV space.
 *          Use %regionflag == L_EXCLUDE_REGION to take all pixels except
 *          those within the rectangular region specified in SV space.
 * </pre>
 */
PIX *
pixMakeRangeMaskSV(PIX     *pixs,
                   l_int32  satcenter,
                   l_int32  sathw,
                   l_int32  valcenter,
                   l_int32  valhw,
                   l_int32  regionflag)
{
l_int32    i, j, w, h, wplt, wpld, sval, vval, sstart, send, vstart, vend;
l_int32   *slut, *vlut;
l_uint32   pixel;
l_uint32  *datat, *datad, *linet, *lined;
PIX       *pixt, *pixd;

    if (!pixs || pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs undefined or not 32 bpp", __func__, NULL);
    if (regionflag != L_INCLUDE_REGION && regionflag != L_EXCLUDE_REGION)
        return (PIX *)ERROR_PTR("invalid regionflag", __func__, NULL);

        /* Set up LUTs for saturation and max intensity (val).
         * These have the value 1 within the specified intervals of
         * saturation and max intensity. */
    slut = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
    vlut = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
    sstart = L_MAX(0, satcenter - sathw);
    send = L_MIN(255, satcenter + sathw);
    vstart = L_MAX(0, valcenter - valhw);
    vend = L_MIN(255, valcenter + valhw);
    for (i = sstart; i <= send; i++)
        slut[i] = 1;
    for (i = vstart; i <= vend; i++)
        vlut[i] = 1;

        /* Generate the mask */
    pixt = pixConvertRGBToHSV(NULL, pixs);
    pixGetDimensions(pixs, &w, &h, NULL);
    pixd = pixCreate(w, h, 1);
    if (regionflag == L_INCLUDE_REGION)
        pixClearAll(pixd);
    else  /* L_EXCLUDE_REGION */
        pixSetAll(pixd);
    datat = pixGetData(pixt);
    datad = pixGetData(pixd);
    wplt = pixGetWpl(pixt);
    wpld = pixGetWpl(pixd);
    for (i = 0; i < h; i++) {
        linet = datat + i * wplt;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            pixel = linet[j];
            sval = (pixel >> L_GREEN_SHIFT) & 0xff;
            vval = (pixel >> L_BLUE_SHIFT) & 0xff;
            if (slut[sval] == 1 && vlut[vval] == 1) {
                if (regionflag == L_INCLUDE_REGION)
                    SET_DATA_BIT(lined, j);
                else  /* L_EXCLUDE_REGION */
                    CLEAR_DATA_BIT(lined, j);
            }
        }
    }

    LEPT_FREE(slut);
    LEPT_FREE(vlut);
    pixDestroy(&pixt);
    return pixd;
}


/*!
 * \brief   pixMakeHistoHS()
 *
 * \param[in]    pixs     HSV colorspace
 * \param[in]    factor   subsampling factor; integer
 * \param[out]   pnahue   [optional] hue histogram
 * \param[out]   pnasat   [optional] saturation histogram
 * \return  pixd   32 bpp histogram in hue and saturation, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) pixs is a 32 bpp image in HSV colorspace; hue is in the "red"
 *          byte, saturation is in the "green" byte.
 *      (2) In pixd, hue is displayed vertically; saturation horizontally.
 *          The dimensions of pixd are w = 256, h = 240, and the depth
 *          is 32 bpp.  The value at each point is simply the number
 *          of pixels found at that value of hue and saturation.
 * </pre>
 */
PIX *
pixMakeHistoHS(PIX     *pixs,
               l_int32  factor,
               NUMA   **pnahue,
               NUMA   **pnasat)
{
l_int32    i, j, w, h, wplt, hval, sval, nd;
l_uint32   pixel;
l_uint32  *datat, *linet;
void     **lined32;
NUMA      *nahue = NULL, *nasat = NULL;
PIX       *pixt, *pixd;

    if (pnahue) *pnahue = NULL;
    if (pnasat) *pnasat = NULL;
    if (!pixs || pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs undefined or not 32 bpp", __func__, NULL);

    if (pnahue) {
        nahue = numaCreate(240);
        numaSetCount(nahue, 240);
        *pnahue = nahue;
    }
    if (pnasat) {
        nasat = numaCreate(256);
        numaSetCount(nasat, 256);
        *pnasat = nasat;
    }

    if (factor <= 1)
        pixt = pixClone(pixs);
    else
        pixt = pixScaleBySampling(pixs, 1.0 / (l_float32)factor,
                                  1.0 / (l_float32)factor);

        /* Create the hue-saturation histogram */
    pixd = pixCreate(256, 240, 32);
    lined32 = pixGetLinePtrs(pixd, NULL);
    pixGetDimensions(pixt, &w, &h, NULL);
    datat = pixGetData(pixt);
    wplt = pixGetWpl(pixt);
    for (i = 0; i < h; i++) {
        linet = datat + i * wplt;
        for (j = 0; j < w; j++) {
            pixel = linet[j];
            hval = (pixel >> L_RED_SHIFT) & 0xff;

#if  DEBUG_HISTO
            if (hval > 239) {
                lept_stderr("hval = %d for (%d,%d)\n", hval, i, j);
                continue;
            }
#endif  /* DEBUG_HISTO */

            sval = (pixel >> L_GREEN_SHIFT) & 0xff;
            if (pnahue)
                numaShiftValue(nahue, hval, 1.0);
            if (pnasat)
                numaShiftValue(nasat, sval, 1.0);
            nd = GET_DATA_FOUR_BYTES(lined32[hval], sval);
            SET_DATA_FOUR_BYTES(lined32[hval], sval, nd + 1);
        }
    }

    LEPT_FREE(lined32);
    pixDestroy(&pixt);
    return pixd;
}


/*!
 * \brief   pixMakeHistoHV()
 *
 * \param[in]    pixs     HSV colorspace
 * \param[in]    factor   subsampling factor; integer
 * \param[out]   pnahue   [optional] hue histogram
 * \param[out]   pnaval   [optional] max intensity (value) histogram
 * \return  pixd   32 bpp histogram in hue and value, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) %pixs is a 32 bpp image in HSV colorspace; hue is in the "red"
 *          byte, max intensity ("value") is in the "blue" byte.
 *      (2) In %pixd, hue is displayed vertically; intensity horizontally.
 *          The dimensions of %pixd are w = 256, h = 240, and the depth
 *          is 32 bpp.  The value at each point is simply the number
 *          of pixels found at that value of hue and intensity.
 * </pre>
 */
PIX *
pixMakeHistoHV(PIX     *pixs,
               l_int32  factor,
               NUMA   **pnahue,
               NUMA   **pnaval)
{
l_int32    i, j, w, h, wplt, hval, vval, nd;
l_uint32   pixel;
l_uint32  *datat, *linet;
void     **lined32;
NUMA      *nahue = NULL, *naval = NULL;
PIX       *pixt, *pixd;

    if (pnahue) *pnahue = NULL;
    if (pnaval) *pnaval = NULL;
    if (!pixs || pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs undefined or not 32 bpp", __func__, NULL);

    if (pnahue) {
        nahue = numaCreate(240);
        numaSetCount(nahue, 240);
        *pnahue = nahue;
    }
    if (pnaval) {
        naval = numaCreate(256);
        numaSetCount(naval, 256);
        *pnaval = naval;
    }

    if (factor <= 1)
        pixt = pixClone(pixs);
    else
        pixt = pixScaleBySampling(pixs, 1.0 / (l_float32)factor,
                                  1.0 / (l_float32)factor);

        /* Create the hue-value histogram */
    pixd = pixCreate(256, 240, 32);
    lined32 = pixGetLinePtrs(pixd, NULL);
    pixGetDimensions(pixt, &w, &h, NULL);
    datat = pixGetData(pixt);
    wplt = pixGetWpl(pixt);
    for (i = 0; i < h; i++) {
        linet = datat + i * wplt;
        for (j = 0; j < w; j++) {
            pixel = linet[j];
            hval = (pixel >> L_RED_SHIFT) & 0xff;
            vval = (pixel >> L_BLUE_SHIFT) & 0xff;
            if (pnahue)
                numaShiftValue(nahue, hval, 1.0);
            if (pnaval)
                numaShiftValue(naval, vval, 1.0);
            nd = GET_DATA_FOUR_BYTES(lined32[hval], vval);
            SET_DATA_FOUR_BYTES(lined32[hval], vval, nd + 1);
        }
    }

    LEPT_FREE(lined32);
    pixDestroy(&pixt);
    return pixd;
}


/*!
 * \brief   pixMakeHistoSV()
 *
 * \param[in]    pixs     HSV colorspace
 * \param[in]    factor   subsampling factor; integer
 * \param[out]   pnasat   [optional] sat histogram
 * \param[out]   pnaval   [optional] max intensity (value) histogram
 * \return  pixd   32 bpp histogram in sat and value, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) %pixs is a 32 bpp image in HSV colorspace; sat is in the "green"
 *          byte, max intensity ("value") is in the "blue" byte.
 *      (2) In %pixd, sat is displayed vertically; intensity horizontally.
 *          The dimensions of %pixd are w = 256, h = 256, and the depth
 *          is 32 bpp.  The value at each point is simply the number
 *          of pixels found at that value of saturation and intensity.
 * </pre>
 */
PIX *
pixMakeHistoSV(PIX     *pixs,
               l_int32  factor,
               NUMA   **pnasat,
               NUMA   **pnaval)
{
l_int32    i, j, w, h, wplt, sval, vval, nd;
l_uint32   pixel;
l_uint32  *datat, *linet;
void     **lined32;
NUMA      *nasat = NULL, *naval = NULL;
PIX       *pixt, *pixd;

    if (pnasat) *pnasat = NULL;
    if (pnaval) *pnaval = NULL;
    if (!pixs || pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs undefined or not 32 bpp", __func__, NULL);

    if (pnasat) {
        nasat = numaCreate(256);
        numaSetCount(nasat, 256);
        *pnasat = nasat;
    }
    if (pnaval) {
        naval = numaCreate(256);
        numaSetCount(naval, 256);
        *pnaval = naval;
    }

    if (factor <= 1)
        pixt = pixClone(pixs);
    else
        pixt = pixScaleBySampling(pixs, 1.0 / (l_float32)factor,
                                  1.0 / (l_float32)factor);

        /* Create the hue-value histogram */
    pixd = pixCreate(256, 256, 32);
    lined32 = pixGetLinePtrs(pixd, NULL);
    pixGetDimensions(pixt, &w, &h, NULL);
    datat = pixGetData(pixt);
    wplt = pixGetWpl(pixt);
    for (i = 0; i < h; i++) {
        linet = datat + i * wplt;
        for (j = 0; j < w; j++) {
            pixel = linet[j];
            sval = (pixel >> L_GREEN_SHIFT) & 0xff;
            vval = (pixel >> L_BLUE_SHIFT) & 0xff;
            if (pnasat)
                numaShiftValue(nasat, sval, 1.0);
            if (pnaval)
                numaShiftValue(naval, vval, 1.0);
            nd = GET_DATA_FOUR_BYTES(lined32[sval], vval);
            SET_DATA_FOUR_BYTES(lined32[sval], vval, nd + 1);
        }
    }

    LEPT_FREE(lined32);
    pixDestroy(&pixt);
    return pixd;
}


/*!
 * \brief   pixFindHistoPeaksHSV()
 *
 * \param[in]    pixs          32 bpp; HS, HV or SV histogram; not changed
 * \param[in]    type          L_HS_HISTO, L_HV_HISTO or L_SV_HISTO
 * \param[in]    width         half width of sliding window
 * \param[in]    height        half height of sliding window
 * \param[in]    npeaks        number of peaks to look for
 * \param[in]    erasefactor   ratio of erase window size to sliding window size
 * \param[out]   ppta          locations of max for each integrated peak area
 * \param[out]   pnatot        integrated peak areas
 * \param[out]   ppixa         [optional] pixa for debugging; NULL to skip
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) %pixs is a 32 bpp histogram in a pair of HSV colorspace.  It
 *          should be thought of as a single sample with 32 bps (bits/sample).
 *      (2) After each peak is found, the peak is erased with a window
 *          that is centered on the peak and scaled from the sliding
 *          window by %erasefactor.  Typically, %erasefactor is chosen
 *          to be > 1.0.
 *      (3) Data for a maximum of %npeaks is returned in %pta and %natot.
 *      (4) For debugging, after the pixa is returned, display with:
 *          pixd = pixaDisplayTiledInRows(pixa, 32, 1000, 1.0, 0, 30, 2);
 * </pre>
 */
l_ok
pixFindHistoPeaksHSV(PIX       *pixs,
                     l_int32    type,
                     l_int32    width,
                     l_int32    height,
                     l_int32    npeaks,
                     l_float32  erasefactor,
                     PTA      **ppta,
                     NUMA     **pnatot,
                     PIXA     **ppixa)
{
l_int32   i, xmax, ymax, ewidth, eheight;
l_uint32  maxval;
BOX      *box;
NUMA     *natot;
PIX      *pixh, *pixw, *pix1, *pix2, *pix3;
PTA      *pta;

    if (ppixa) *ppixa = NULL;
    if (ppta) *ppta = NULL;
    if (pnatot) *pnatot = NULL;
    if (!pixs || pixGetDepth(pixs) != 32)
        return ERROR_INT("pixs undefined or not 32 bpp", __func__, 1);
    if (!ppta || !pnatot)
        return ERROR_INT("&pta and &natot not both defined", __func__, 1);
    if (type != L_HS_HISTO && type != L_HV_HISTO && type != L_SV_HISTO)
        return ERROR_INT("invalid HSV histo type", __func__, 1);

    if ((pta = ptaCreate(npeaks)) == NULL)
        return ERROR_INT("pta not made", __func__, 1);
    *ppta = pta;
    if ((natot = numaCreate(npeaks)) == NULL)
        return ERROR_INT("natot not made", __func__, 1);
    *pnatot = natot;

    *ppta = pta;
    if (type == L_SV_HISTO)
        pixh = pixAddMirroredBorder(pixs, width + 1, width + 1, height + 1,
                                    height + 1);
    else  /* type == L_HS_HISTO or type == L_HV_HISTO */
        pixh = pixAddMixedBorder(pixs, width + 1, width + 1, height + 1,
                                 height + 1);

        /* Get the total count in the sliding window.  If the window
         * fully covers the peak, this will be the integrated
         * volume under the peak. */
    pixw = pixWindowedMean(pixh, width, height, 1, 0);
    pixDestroy(&pixh);

        /* Sequentially identify and erase peaks in the histogram.
         * If requested for debugging, save a pixa of the sequence of
         * false color histograms. */
    if (ppixa)
        *ppixa = pixaCreate(0);
    for (i = 0; i < npeaks; i++) {
        pixGetMaxValueInRect(pixw, NULL, &maxval, &xmax, &ymax);
        if (maxval == 0) break;
        numaAddNumber(natot, maxval);
        ptaAddPt(pta, xmax, ymax);
        ewidth = (l_int32)(width * erasefactor);
        eheight = (l_int32)(height * erasefactor);
        box = boxCreate(xmax - ewidth, ymax - eheight, 2 * ewidth + 1,
                        2 * eheight + 1);

        if (ppixa) {
            pix1 = pixMaxDynamicRange(pixw, L_LINEAR_SCALE);
            pixaAddPix(*ppixa, pix1, L_INSERT);
            pix2 = pixConvertGrayToFalseColor(pix1, 1.0);
            pixaAddPix(*ppixa, pix2, L_INSERT);
            pix1 = pixMaxDynamicRange(pixw, L_LOG_SCALE);
            pix2 = pixConvertGrayToFalseColor(pix1, 1.0);
            pixaAddPix(*ppixa, pix2, L_INSERT);
            pix3 = pixConvertTo32(pix1);
            pixRenderHashBoxArb(pix3, box, 6, 2, L_NEG_SLOPE_LINE,
                                1, 255, 100, 100);
            pixaAddPix(*ppixa, pix3, L_INSERT);
            pixDestroy(&pix1);
        }

        pixClearInRect(pixw, box);
        boxDestroy(&box);
        if (type == L_HS_HISTO || type == L_HV_HISTO) {
                /* clear wraps at bottom and top */
            if (ymax - eheight < 0) {  /* overlap to bottom */
                box = boxCreate(xmax - ewidth, 240 + ymax - eheight,
                                2 * ewidth + 1, eheight - ymax);
            } else if (ymax + eheight > 239) {  /* overlap to top */
                box = boxCreate(xmax - ewidth, 0, 2 * ewidth + 1,
                                ymax + eheight - 239);
            } else {
                box = NULL;
            }
            if (box) {
                pixClearInRect(pixw, box);
                boxDestroy(&box);
            }
        }
    }

    pixDestroy(&pixw);
    return 0;
}


/*!
 * \brief   displayHSVColorRange()
 *
 * \param[in]    hval     hue center value; in range [0 ... 240]
 * \param[in]    sval     saturation center value; in range [0 ... 255]
 * \param[in]    vval     max intensity value; in range [0 ... 255]
 * \param[in]    huehw    half-width of hue range; > 0
 * \param[in]    sathw    half-width of saturation range; > 0
 * \param[in]    nsamp    number of samplings in each half-width in hue and sat
 * \param[in]    factor   linear size of each color square, in pixels; > 3
 * \return  pixd   32 bpp set of color squares over input range; NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The total number of color samplings in each of the hue
 *          and saturation directions is 2 * nsamp + 1.
 * </pre>
 */
PIX *
displayHSVColorRange(l_int32  hval,
                     l_int32  sval,
                     l_int32  vval,
                     l_int32  huehw,
                     l_int32  sathw,
                     l_int32  nsamp,
                     l_int32  factor)
{
l_int32  i, j, w, huedelta, satdelta, hue, sat, rval, gval, bval;
PIX     *pixt, *pixd;

    if (hval < 0 || hval > 240)
        return (PIX *)ERROR_PTR("invalid hval", __func__, NULL);
    if (huehw < 5 || huehw > 120)
        return (PIX *)ERROR_PTR("invalid huehw", __func__, NULL);
    if (sval - sathw < 0 || sval + sathw > 255)
        return (PIX *)ERROR_PTR("invalid sval/sathw", __func__, NULL);
    if (nsamp < 1 || factor < 3)
        return (PIX *)ERROR_PTR("invalid nsamp or rep. factor", __func__, NULL);
    if (vval < 0 || vval > 255)
        return (PIX *)ERROR_PTR("invalid vval", __func__, NULL);

    w = (2 * nsamp + 1);
    huedelta = (l_int32)((l_float32)huehw / (l_float32)nsamp);
    satdelta = (l_int32)((l_float32)sathw / (l_float32)nsamp);
    pixt = pixCreate(w, w, 32);
    for (i = 0; i < w; i++) {
        hue = hval + huedelta * (i - nsamp);
        if (hue < 0) hue += 240;
        if (hue >= 240) hue -= 240;
        for (j = 0; j < w; j++) {
            sat = sval + satdelta * (j - nsamp);
            convertHSVToRGB(hue, sat, vval, &rval, &gval, &bval);
            pixSetRGBPixel(pixt, j, i, rval, gval, bval);
        }
    }

    pixd = pixExpandReplicate(pixt, factor);
    pixDestroy(&pixt);
    return pixd;
}


/*---------------------------------------------------------------------------*
 *                Colorspace conversion between RGB and YUV                  *
 *---------------------------------------------------------------------------*/
/*!
 * \brief   pixConvertRGBToYUV()
 *
 * \param[in]    pixd   can be NULL; if not NULL, must == pixs
 * \param[in]    pixs
 * \return  pixd always
 *
 * <pre>
 * Notes:
 *      (1) For pixs = pixd, this is in-place; otherwise pixd must be NULL.
 *      (2) The Y, U and V values are stored in the same places as
 *          the r, g and b values, respectively.  Here, they are explicitly
 *          placed in the 3 MS bytes in the pixel.
 *      (3) Normalizing to 1 and considering the r,g,b components,
 *          a simple way to understand the YUV space is:
 *           ~ Y = weighted sum of (r,g,b)
 *           ~ U = weighted difference between Y and B
 *           ~ V = weighted difference between Y and R
 *      (4) Following video conventions, Y, U and V are in the range:
 *             Y: [16, 235]
 *             U: [16, 240]
 *             V: [16, 240]
 *      (5) For the coefficients in the transform matrices, see eq. 4 in
 *          "Frequently Asked Questions about Color" by Charles Poynton,
 *          //http://user.engineering.uiowa.edu/~aip/Misc/ColorFAQ.html
 * </pre>
 */
PIX *
pixConvertRGBToYUV(PIX  *pixd,
                   PIX  *pixs)
{
l_int32    w, h, d, wpl, i, j, rval, gval, bval, yval, uval, vval;
l_uint32  *line, *data;
PIXCMAP   *cmap;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, pixd);
    if (pixd && pixd != pixs)
        return (PIX *)ERROR_PTR("pixd defined and not inplace", __func__, pixd);

    d = pixGetDepth(pixs);
    cmap = pixGetColormap(pixs);
    if (!cmap && d != 32)
        return (PIX *)ERROR_PTR("not cmapped or rgb", __func__, pixd);

    if (!pixd)
        pixd = pixCopy(NULL, pixs);

    cmap = pixGetColormap(pixd);
    if (cmap) {   /* just convert the colormap */
        pixcmapConvertRGBToYUV(cmap);
        return pixd;
    }

        /* Convert RGB image */
    pixGetDimensions(pixd, &w, &h, NULL);
    wpl = pixGetWpl(pixd);
    data = pixGetData(pixd);
    for (i = 0; i < h; i++) {
        line = data + i * wpl;
        for (j = 0; j < w; j++) {
            extractRGBValues(line[j], &rval, &gval, &bval);
            convertRGBToYUV(rval, gval, bval, &yval, &uval, &vval);
            line[j] = (yval << 24) | (uval << 16) | (vval << 8);
        }
    }

    return pixd;
}


/*!
 * \brief   pixConvertYUVToRGB()
 *
 * \param[in]    pixd   can be NULL; if not NULL, must == pixs
 * \param[in]    pixs
 * \return  pixd always
 *
 * <pre>
 * Notes:
 *      (1) For pixs = pixd, this is in-place; otherwise pixd must be NULL.
 *      (2) The user takes responsibility for making sure that pixs is
 *          in YUV space.
 *      (3) The Y, U and V values are stored in the same places as
 *          the r, g and b values, respectively.  Here, they are explicitly
 *          placed in the 3 MS bytes in the pixel.
 * </pre>
 */
PIX *
pixConvertYUVToRGB(PIX  *pixd,
                   PIX  *pixs)
{
l_int32    w, h, d, wpl, i, j, rval, gval, bval, yval, uval, vval;
l_uint32   pixel;
l_uint32  *line, *data;
PIXCMAP   *cmap;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, pixd);
    if (pixd && pixd != pixs)
        return (PIX *)ERROR_PTR("pixd defined and not inplace", __func__, pixd);

    d = pixGetDepth(pixs);
    cmap = pixGetColormap(pixs);
    if (!cmap && d != 32)
        return (PIX *)ERROR_PTR("not cmapped or hsv", __func__, pixd);

    if (!pixd)
        pixd = pixCopy(NULL, pixs);

    cmap = pixGetColormap(pixd);
    if (cmap) {   /* just convert the colormap */
        pixcmapConvertYUVToRGB(cmap);
        return pixd;
    }

        /* Convert YUV image */
    pixGetDimensions(pixd, &w, &h, NULL);
    wpl = pixGetWpl(pixd);
    data = pixGetData(pixd);
    for (i = 0; i < h; i++) {
        line = data + i * wpl;
        for (j = 0; j < w; j++) {
            pixel = line[j];
            yval = pixel >> 24;
            uval = (pixel >> 16) & 0xff;
            vval = (pixel >> 8) & 0xff;
            convertYUVToRGB(yval, uval, vval, &rval, &gval, &bval);
            composeRGBPixel(rval, gval, bval, line + j);
        }
    }

    return pixd;
}


/*!
 * \brief   convertRGBToYUV()
 *
 * \param[in]    rval, gval, bval      RGB input
 * \param[out]   pyval, puval, pvval   equivalent YUV values
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The range of returned values is:
 *            Y [16 ... 235]
 *            U [16 ... 240]
 *            V [16 ... 240]
 * </pre>
 */
l_ok
convertRGBToYUV(l_int32   rval,
                l_int32   gval,
                l_int32   bval,
                l_int32  *pyval,
                l_int32  *puval,
                l_int32  *pvval)
{
l_float32  norm;

    if (pyval) *pyval = 0;
    if (puval) *puval = 0;
    if (pvval) *pvval = 0;
    if (!pyval || !puval || !pvval)
        return ERROR_INT("&yval, &uval, &vval not all defined", __func__, 1);

    norm = 1.0 / 256.;
    *pyval = (l_int32)(16.0 +
                norm * (65.738 * rval + 129.057 * gval + 25.064 * bval) + 0.5);
    *puval = (l_int32)(128.0 +
                norm * (-37.945 * rval -74.494 * gval + 112.439 * bval) + 0.5);
    *pvval = (l_int32)(128.0 +
                norm * (112.439 * rval - 94.154 * gval - 18.285 * bval) + 0.5);
    return 0;
}


/*!
 * \brief   convertYUVToRGB()
 *
 * \param[in]    yval, uval, vval      YUV input
 * \param[out]   prval, pgval, pbval   equivalent RGB values
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The range of valid input values is:
 *            Y [16 ... 235]
 *            U [16 ... 240]
 *            V [16 ... 240]
 *      (2) Conversion of RGB --> YUV --> RGB leaves the image unchanged.
 *      (3) The YUV gamut is larger than the RBG gamut; many YUV values
 *          will result in an invalid RGB value.  We clip individual
 *          r,g,b components to the range [0, 255], and do not test input.
 * </pre>
 */
l_ok
convertYUVToRGB(l_int32   yval,
                l_int32   uval,
                l_int32   vval,
                l_int32  *prval,
                l_int32  *pgval,
                l_int32  *pbval)
{
l_int32    rval, gval, bval;
l_float32  norm, ym, um, vm;

    if (prval) *prval = 0;
    if (pgval) *pgval = 0;
    if (pbval) *pbval = 0;
    if (!prval || !pgval || !pbval)
        return ERROR_INT("&rval, &gval, &bval not all defined", __func__, 1);

    norm = 1.0 / 256.;
    ym = yval - 16.0;
    um = uval - 128.0;
    vm = vval - 128.0;
    rval =  (l_int32)(norm * (298.082 * ym + 408.583 * vm) + 0.5);
    gval = (l_int32)(norm * (298.082 * ym - 100.291 * um - 208.120 * vm) +
                       0.5);
    bval = (l_int32)(norm * (298.082 * ym + 516.411 * um) + 0.5);
    *prval = L_MIN(255, L_MAX(0, rval));
    *pgval = L_MIN(255, L_MAX(0, gval));
    *pbval = L_MIN(255, L_MAX(0, bval));

    return 0;
}


/*!
 * \brief   pixcmapConvertRGBToYUV()
 *
 * \param[in]    cmap
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      ~ in-place transform
 *      ~ See convertRGBToYUV() for def'n of YUV space.
 *      ~ replaces: r --> y, g --> u, b --> v
 * </pre>
 */
l_ok
pixcmapConvertRGBToYUV(PIXCMAP  *cmap)
{
l_int32   i, ncolors, rval, gval, bval, yval, uval, vval;

    if (!cmap)
        return ERROR_INT("cmap not defined", __func__, 1);

    ncolors = pixcmapGetCount(cmap);
    for (i = 0; i < ncolors; i++) {
        pixcmapGetColor(cmap, i, &rval, &gval, &bval);
        convertRGBToYUV(rval, gval, bval, &yval, &uval, &vval);
        pixcmapResetColor(cmap, i, yval, uval, vval);
    }
    return 0;
}


/*!
 * \brief   pixcmapConvertYUVToRGB()
 *
 * \param[in]    cmap
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      ~ in-place transform
 *      ~ See convertRGBToYUV() for def'n of YUV space.
 *      ~ replaces: y --> r, u --> g, v --> b
 * </pre>
 */
l_ok
pixcmapConvertYUVToRGB(PIXCMAP  *cmap)
{
l_int32   i, ncolors, rval, gval, bval, yval, uval, vval;

    if (!cmap)
        return ERROR_INT("cmap not defined", __func__, 1);

    ncolors = pixcmapGetCount(cmap);
    for (i = 0; i < ncolors; i++) {
        pixcmapGetColor(cmap, i, &yval, &uval, &vval);
        convertYUVToRGB(yval, uval, vval, &rval, &gval, &bval);
        pixcmapResetColor(cmap, i, rval, gval, bval);
    }
    return 0;
}


/*---------------------------------------------------------------------------*
 *                Colorspace conversion between RGB and XYZ                  *
 *---------------------------------------------------------------------------*/
/*!
 * \brief   pixConvertRGBToXYZ()
 *
 * \param[in]    pixs    32 bpp rgb
 * \return  fpixa xyz
 *
 * <pre>
 * Notes:
 *      (1) The [x,y,z] values are stored as float values in three fpix
 *          that are returned in a fpixa.
 *      (2) The XYZ color space was defined in 1931 as a reference model that
 *          simulates human color perception.  When Y is taken as luminance,
 *          the values of X and Z constitute a color plane representing
 *          all the hues that can be perceived.  This gamut of colors
 *          is larger than the gamuts that can be displayed or printed.
 *          For example, although all rgb values map to XYZ, the converse
 *          is not true.
 *      (3) The value of the coefficients depends on the illuminant.  We use
 *          coefficients for converting sRGB under D65 (the spectrum from
 *          a 6500 degree K black body; an approximation to daylight color).
 *          See, e.g.,
 *             http://www.cs.rit.edu/~ncs/color/t_convert.html
 *          For more general information on color transforms, see:
 *             http://www.brucelindbloom.com/
 *             http://user.engineering.uiowa.edu/~aip/Misc/ColorFAQ.html
 *             http://en.wikipedia.org/wiki/CIE_1931_color_space
 * </pre>
 */
FPIXA *
pixConvertRGBToXYZ(PIX  *pixs)
{
l_int32     w, h, wpls, wpld, i, j, rval, gval, bval;
l_uint32   *lines, *datas;
l_float32   fxval, fyval, fzval;
l_float32  *linex, *liney, *linez, *datax, *datay, *dataz;
FPIX       *fpix;
FPIXA      *fpixa;

    if (!pixs || pixGetDepth(pixs) != 32)
        return (FPIXA *)ERROR_PTR("pixs undefined or not rgb", __func__, NULL);

        /* Convert RGB image */
    pixGetDimensions(pixs, &w, &h, NULL);
    fpixa = fpixaCreate(3);
    for (i = 0; i < 3; i++) {
        fpix = fpixCreate(w, h);
        fpixaAddFPix(fpixa, fpix, L_INSERT);
    }
    wpls = pixGetWpl(pixs);
    wpld = fpixGetWpl(fpix);
    datas = pixGetData(pixs);
    datax = fpixaGetData(fpixa, 0);
    datay = fpixaGetData(fpixa, 1);
    dataz = fpixaGetData(fpixa, 2);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        linex = datax + i * wpld;
        liney = datay + i * wpld;
        linez = dataz + i * wpld;
        for (j = 0; j < w; j++) {
            extractRGBValues(lines[j], &rval, &gval, &bval);
            convertRGBToXYZ(rval, gval, bval, &fxval, &fyval, &fzval);
            *(linex + j) = fxval;
            *(liney + j) = fyval;
            *(linez + j) = fzval;
        }
    }

    return fpixa;
}


/*!
 * \brief   fpixaConvertXYZToRGB()
 *
 * \param[in]    fpixa    three fpix: x,y,z
 * \return  pixd   32 bpp rgb
 *
 * <pre>
 * Notes:
 *      (1) The xyz image is stored in three fpix.
 *      (2) For values of xyz that are out of gamut for rgb, the rgb
 *          components are set to the closest valid color.
 * </pre>
 */
PIX *
fpixaConvertXYZToRGB(FPIXA  *fpixa)
{
l_int32     w, h, wpls, wpld, i, j, rval, gval, bval;
l_float32   fxval, fyval, fzval;
l_float32  *linex, *liney, *linez, *datax, *datay, *dataz;
l_uint32   *lined, *datad;
PIX        *pixd;
FPIX       *fpix;

    if (!fpixa || fpixaGetCount(fpixa) != 3)
        return (PIX *)ERROR_PTR("fpixa undefined or invalid", __func__, NULL);

        /* Convert XYZ image */
    if (fpixaGetFPixDimensions(fpixa, 0, &w, &h))
        return (PIX *)ERROR_PTR("fpixa dimensions not found", __func__, NULL);
    pixd = pixCreate(w, h, 32);
    wpld = pixGetWpl(pixd);
    datad = pixGetData(pixd);
    datax = fpixaGetData(fpixa, 0);
    datay = fpixaGetData(fpixa, 1);
    dataz = fpixaGetData(fpixa, 2);
    fpix = fpixaGetFPix(fpixa, 0, L_CLONE);
    wpls = fpixGetWpl(fpix);
    fpixDestroy(&fpix);
    for (i = 0; i < h; i++) {
        linex = datax + i * wpls;
        liney = datay + i * wpls;
        linez = dataz + i * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            fxval = linex[j];
            fyval = liney[j];
            fzval = linez[j];
            convertXYZToRGB(fxval, fyval, fzval, 0, &rval, &gval, &bval);
            composeRGBPixel(rval, gval, bval, lined + j);
        }
    }

    return pixd;
}


/*!
 * \brief   convertRGBToXYZ()
 *
 * \param[in]    rval, gval, bval         rgb input
 * \param[out]   pfxval, pfyval, pfzval   equivalent xyz values
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) These conversions are for illuminant D65 acting on linear sRGB
 *          values.
 * </pre>
 */
l_ok
convertRGBToXYZ(l_int32     rval,
                l_int32     gval,
                l_int32     bval,
                l_float32  *pfxval,
                l_float32  *pfyval,
                l_float32  *pfzval)
{
    if (pfxval) *pfxval = 0.0;
    if (pfyval) *pfyval = 0.0;
    if (pfzval) *pfzval = 0.0;
    if (!pfxval || !pfyval || !pfzval)
        return ERROR_INT("&xval, &yval, &zval not all defined", __func__, 1);

    *pfxval = 0.4125 * rval + 0.3576 * gval + 0.1804 * bval;
    *pfyval = 0.2127 * rval + 0.7152 * gval + 0.0722 * bval;
    *pfzval = 0.0193 * rval + 0.1192 * gval + 0.9502 * bval;
    return 0;
}


/*!
 * \brief   convertXYZToRGB()
 *
 * \param[in]    fxval, fyval, fzval
 * \param[in]    blackout    0 to output nearest color if out of gamut;
 *                           1 to output black
 * \param[out]   prval, pgval, pbval   32 bpp rgb values
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) For values of xyz that are out of gamut for rgb, at least
 *          one of the r, g or b components will be either less than 0
 *          or greater than 255.  For that situation:
 *            * if %blackout == 0, the individual component(s) that are out
 *              of gamut will be set to 0 or 255, respectively.
 *            * if %blackout == 1, the output color will be set to black
 * </pre>
 */
l_ok
convertXYZToRGB(l_float32  fxval,
                l_float32  fyval,
                l_float32  fzval,
                l_int32    blackout,
                l_int32   *prval,
                l_int32   *pgval,
                l_int32   *pbval)
{
l_int32  rval, gval, bval;

    if (prval) *prval = 0;
    if (pgval) *pgval = 0;
    if (pbval) *pbval = 0;
    if (!prval || !pgval ||!pbval)
        return ERROR_INT("&rval, &gval, &bval not all defined", __func__, 1);
    *prval = *pgval = *pbval = 0;

    rval = (l_int32)(3.2405 * fxval - 1.5372 * fyval - 0.4985 * fzval + 0.5);
    gval = (l_int32)(-0.9693 * fxval + 1.8760 * fyval + 0.0416 * fzval + 0.5);
    bval = (l_int32)(0.0556 * fxval - 0.2040 * fyval + 1.0573 * fzval + 0.5);
    if (blackout == 0) {  /* the usual situation; use nearest rgb color */
        *prval = L_MAX(0, L_MIN(rval, 255));
        *pgval = L_MAX(0, L_MIN(gval, 255));
        *pbval = L_MAX(0, L_MIN(bval, 255));
    } else {  /* use black for out of gamut */
        if (rval >= 0 && rval < 256 && gval >= 0 && gval < 256 &&
            bval >= 0 && bval < 256) {  /* in gamut */
            *prval = rval;
            *pgval = gval;
            *pbval = bval;
        }
    }
    return 0;
}


/*---------------------------------------------------------------------------*
 *               Colorspace conversion between XYZ and LAB                   *
 *---------------------------------------------------------------------------*/
/*!
 * \brief   fpixaConvertXYZToLAB()
 *
 * \param[in]    fpixas    xyz
 * \return  fpixa lab
 *
 * <pre>
 * Notes:
 *      (1) The input [x,y,z] and output [l,a,b] values are stored as
 *          float values, each set in three fpix.
 *      (2) The CIE LAB color space was invented in 1976, as an
 *          absolute reference for specifying colors that we can
 *          perceive, independently of the rendering device.  It was
 *          invented to align color display and print images.
 *          For information, see:
 *             http://www.brucelindbloom.com/
 *             http://en.wikipedia.org/wiki/Lab_color_space
 * </pre>
 */
FPIXA *
fpixaConvertXYZToLAB(FPIXA  *fpixas)
{
l_int32     w, h, wpl, i, j;
l_float32   fxval, fyval, fzval, flval, faval, fbval;
l_float32  *linex, *liney, *linez, *datax, *datay, *dataz;
l_float32  *linel, *linea, *lineb, *datal, *dataa, *datab;
FPIX       *fpix;
FPIXA      *fpixad;

    if (!fpixas || fpixaGetCount(fpixas) != 3)
        return (FPIXA *)ERROR_PTR("fpixas undefined/invalid", __func__, NULL);

        /* Convert XYZ image */
    if (fpixaGetFPixDimensions(fpixas, 0, &w, &h))
        return (FPIXA *)ERROR_PTR("fpixas sizes not found", __func__, NULL);
    fpixad = fpixaCreate(3);
    for (i = 0; i < 3; i++) {
        fpix = fpixCreate(w, h);
        fpixaAddFPix(fpixad, fpix, L_INSERT);
    }
    wpl = fpixGetWpl(fpix);
    datax = fpixaGetData(fpixas, 0);
    datay = fpixaGetData(fpixas, 1);
    dataz = fpixaGetData(fpixas, 2);
    datal = fpixaGetData(fpixad, 0);
    dataa = fpixaGetData(fpixad, 1);
    datab = fpixaGetData(fpixad, 2);

        /* Convert XYZ image */
    for (i = 0; i < h; i++) {
        linex = datax + i * wpl;
        liney = datay + i * wpl;
        linez = dataz + i * wpl;
        linel = datal + i * wpl;
        linea = dataa + i * wpl;
        lineb = datab + i * wpl;
        for (j = 0; j < w; j++) {
            fxval = *(linex + j);
            fyval = *(liney + j);
            fzval = *(linez + j);
            convertXYZToLAB(fxval, fyval, fzval, &flval, &faval, &fbval);
            *(linel + j) = flval;
            *(linea + j) = faval;
            *(lineb + j) = fbval;
        }
    }

    return fpixad;
}


/*!
 * \brief   fpixaConvertLABToXYZ()
 *
 * \param[in]    fpixas    lab
 * \return  fpixa    xyz
 *
 * <pre>
 * Notes:
 *      (1) The input [l,a,b] and output [x,y,z] values are stored as
 *          float values, each set in three fpix.
 * </pre>
 */
FPIXA *
fpixaConvertLABToXYZ(FPIXA  *fpixas)
{
l_int32     w, h, wpl, i, j;
l_float32   fxval, fyval, fzval, flval, faval, fbval;
l_float32  *linel, *linea, *lineb, *datal, *dataa, *datab;
l_float32  *linex, *liney, *linez, *datax, *datay, *dataz;
FPIX       *fpix;
FPIXA      *fpixad;

    if (!fpixas || fpixaGetCount(fpixas) != 3)
        return (FPIXA *)ERROR_PTR("fpixas undefined/invalid", __func__, NULL);

        /* Convert LAB image */
    if (fpixaGetFPixDimensions(fpixas, 0, &w, &h))
        return (FPIXA *)ERROR_PTR("fpixas sizes not found", __func__, NULL);
    fpixad = fpixaCreate(3);
    for (i = 0; i < 3; i++) {
        fpix = fpixCreate(w, h);
        fpixaAddFPix(fpixad, fpix, L_INSERT);
    }
    wpl = fpixGetWpl(fpix);
    datal = fpixaGetData(fpixas, 0);
    dataa = fpixaGetData(fpixas, 1);
    datab = fpixaGetData(fpixas, 2);
    datax = fpixaGetData(fpixad, 0);
    datay = fpixaGetData(fpixad, 1);
    dataz = fpixaGetData(fpixad, 2);

        /* Convert XYZ image */
    for (i = 0; i < h; i++) {
        linel = datal + i * wpl;
        linea = dataa + i * wpl;
        lineb = datab + i * wpl;
        linex = datax + i * wpl;
        liney = datay + i * wpl;
        linez = dataz + i * wpl;
        for (j = 0; j < w; j++) {
            flval = *(linel + j);
            faval = *(linea + j);
            fbval = *(lineb + j);
            convertLABToXYZ(flval, faval, fbval, &fxval, &fyval, &fzval);
            *(linex + j) = fxval;
            *(liney + j) = fyval;
            *(linez + j) = fzval;
        }
    }

    return fpixad;
}


/*!
 * \brief   convertXYZToLAB()
 *
 * \param[in]    xval, yval, zval      input xyz
 * \param[out]   plval, paval, pbval   equivalent lab values
 * \return  0 if OK, 1 on error
 */
l_ok
convertXYZToLAB(l_float32   xval,
                l_float32   yval,
                l_float32   zval,
                l_float32  *plval,
                l_float32  *paval,
                l_float32  *pbval)
{
l_float32  xn, yn, zn, fx, fy, fz;

    if (plval) *plval = 0.0;
    if (paval) *paval = 0.0;
    if (pbval) *pbval = 0.0;
    if (!plval || !paval || !pbval)
        return ERROR_INT("&lval, &aval, &bval not all defined", __func__, 1);

        /* First normalize to the corresponding white values */
    xn = 0.0041259 * xval;
    yn = 0.0039216 * yval;
    zn = 0.0036012 * zval;
        /* Then apply the lab_forward function */
    fx = lab_forward(xn);
    fy = lab_forward(yn);
    fz = lab_forward(zn);
    *plval = 116.0 * fy - 16.0;
    *paval = 500.0 * (fx - fy);
    *pbval = 200.0 * (fy - fz);
    return 0;
}


/*!
 * \brief   convertLABToXYZ()
 *
 * \param[in]    lval, aval, bval      input lab
 * \param[out]   pxval, pyval, pzval   equivalent xyz values
 * \return  0 if OK, 1 on error
 */
l_ok
convertLABToXYZ(l_float32   lval,
                l_float32   aval,
                l_float32   bval,
                l_float32  *pxval,
                l_float32  *pyval,
                l_float32  *pzval)
{
l_float32  fx, fy, fz;
l_float32  xw = 242.37f;  /* x component corresponding to rgb white */
l_float32  yw = 255.0f;   /* y component corresponding to rgb white */
l_float32  zw = 277.69f;  /* z component corresponding to rgb white */

    if (pxval) *pxval = 0.0;
    if (pyval) *pyval = 0.0;
    if (pzval) *pzval = 0.0;
    if (!pxval || !pyval || !pzval)
        return ERROR_INT("&xval, &yval, &zval not all defined", __func__, 1);

    fy = 0.0086207 * (16.0 + lval);
    fx = fy + 0.002 * aval;
    fz = fy - 0.005 * bval;
    *pxval = xw * lab_reverse(fx);
    *pyval = yw * lab_reverse(fy);
    *pzval = zw * lab_reverse(fz);
    return 0;
}


/*
 * See http://en.wikipedia.org/wiki/Lab_color_space for formulas.
 * This is the forward function: from xyz to lab.  It includes a rational
 * function approximation over [0.008856 ... 1] to the cube root, from
 * "Fast Color Space Transformations Using Minimax Approximations",
 * M. Celebi et al, http://arxiv.org/pdf/1009.0854v1.pdf.
 */
static l_float32
lab_forward(l_float32  v)
{
const l_float32  f_thresh = 0.008856f;  /* (6/29)^3  */
const l_float32  f_factor = 7.787f;     /* (1/3) * (29/6)^2)  */
const l_float32  f_offset = 0.13793f;   /* 4/29 */

    if (v > f_thresh) {
#if  SLOW_CUBE_ROOT
        return powf(v, 0.333333);
#else
        l_float32  num, den;
        num = 4.37089e-04 + v * (9.52695e-02 + v * (1.25201 + v * 1.30273));
        den = 3.91236e-03 + v * (2.95408e-01 + v * (1.71714 + v * 6.34341e-01));
        return num / den;
#endif
    } else {
        return f_factor * v + f_offset;
    }
}


/*
 * See http://en.wikipedia.org/wiki/Lab_color_space for formulas.
 * This is the reverse (inverse) function: from lab to xyz.
 */
static l_float32
lab_reverse(l_float32  v)
{
const l_float32  r_thresh = 0.20690f;  /* 6/29  */
const l_float32  r_factor = 0.12842f;  /* 3 * (6/29)^2   */
const l_float32  r_offset = 0.13793f;  /* 4/29 */

    if (v > r_thresh) {
        return v * v * v;
    } else {
        return r_factor * (v - r_offset);
    }
}


/*---------------------------------------------------------------------------*
 *               Colorspace conversion between RGB and LAB                   *
 *---------------------------------------------------------------------------*/
/*!
 * \brief   pixConvertRGBToLAB()
 *
 * \param[in]    pixs   32 bpp rgb
 * \return  fpixa lab
 *
 * <pre>
 * Notes:
 *      (1) The [l,a,b] values are stored as float values in three fpix
 *          that are returned in a fpixa.
 * </pre>
 */
FPIXA *
pixConvertRGBToLAB(PIX  *pixs)
{
l_int32     w, h, wpls, wpld, i, j, rval, gval, bval;
l_uint32   *lines, *datas;
l_float32   flval, faval, fbval;
l_float32  *linel, *linea, *lineb, *datal, *dataa, *datab;
FPIX       *fpix;
FPIXA      *fpixa;

    if (!pixs || pixGetDepth(pixs) != 32)
        return (FPIXA *)ERROR_PTR("pixs undefined or not rgb", __func__, NULL);

        /* Convert RGB image */
    pixGetDimensions(pixs, &w, &h, NULL);
    fpixa = fpixaCreate(3);
    for (i = 0; i < 3; i++) {
        fpix = fpixCreate(w, h);
        fpixaAddFPix(fpixa, fpix, L_INSERT);
    }
    wpls = pixGetWpl(pixs);
    wpld = fpixGetWpl(fpix);
    datas = pixGetData(pixs);
    datal = fpixaGetData(fpixa, 0);
    dataa = fpixaGetData(fpixa, 1);
    datab = fpixaGetData(fpixa, 2);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        linel = datal + i * wpld;
        linea = dataa + i * wpld;
        lineb = datab + i * wpld;
        for (j = 0; j < w; j++) {
            extractRGBValues(lines[j], &rval, &gval, &bval);
            convertRGBToLAB(rval, gval, bval, &flval, &faval, &fbval);
            *(linel + j) = flval;
            *(linea + j) = faval;
            *(lineb + j) = fbval;
        }
    }

    return fpixa;
}


/*!
 * \brief   fpixaConvertLABToRGB()
 *
 * \param[in]    fpixa    three fpix: l,a,b
 * \return  pixd  32 bpp rgb
 *
 * <pre>
 * Notes:
 *      (1) The lab image is stored in three fpix.
 * </pre>
 */
PIX *
fpixaConvertLABToRGB(FPIXA  *fpixa)
{
l_int32     w, h, wpls, wpld, i, j, rval, gval, bval;
l_float32   flval, faval, fbval;
l_float32  *linel, *linea, *lineb, *datal, *dataa, *datab;
l_uint32   *lined, *datad;
PIX        *pixd;
FPIX       *fpix;

    if (!fpixa || fpixaGetCount(fpixa) != 3)
        return (PIX *)ERROR_PTR("fpixa undefined or invalid", __func__, NULL);

        /* Convert LAB image */
    if (fpixaGetFPixDimensions(fpixa, 0, &w, &h))
        return (PIX *)ERROR_PTR("fpixa dimensions not found", __func__, NULL);
    pixd = pixCreate(w, h, 32);
    wpld = pixGetWpl(pixd);
    datad = pixGetData(pixd);
    datal = fpixaGetData(fpixa, 0);
    dataa = fpixaGetData(fpixa, 1);
    datab = fpixaGetData(fpixa, 2);
    fpix = fpixaGetFPix(fpixa, 0, L_CLONE);
    wpls = fpixGetWpl(fpix);
    fpixDestroy(&fpix);
    for (i = 0; i < h; i++) {
        linel = datal + i * wpls;
        linea = dataa + i * wpls;
        lineb = datab + i * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            flval = linel[j];
            faval = linea[j];
            fbval = lineb[j];
            convertLABToRGB(flval, faval, fbval, &rval, &gval, &bval);
            composeRGBPixel(rval, gval, bval, lined + j);
        }
    }

    return pixd;
}


/*!
 * \brief   convertRGBToLAB()
 *
 * \param[in]    rval, gval, bval        rgb input
 * \param[out]   pflval, pfaval, pfbval  equivalent lab values
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) These conversions are for illuminant D65 acting on linear sRGB
 *          values.
 * </pre>
 */
l_ok
convertRGBToLAB(l_int32     rval,
                l_int32     gval,
                l_int32     bval,
                l_float32  *pflval,
                l_float32  *pfaval,
                l_float32  *pfbval)
{
l_float32  fxval, fyval, fzval;

    if (pflval) *pflval = 0.0;
    if (pfaval) *pfaval = 0.0;
    if (pfbval) *pfbval = 0.0;
    if (!pflval || !pfaval || !pfbval)
        return ERROR_INT("&flval, &faval, &fbval not all defined", __func__, 1);

    convertRGBToXYZ(rval, gval, bval, &fxval, &fyval, &fzval);
    convertXYZToLAB(fxval, fyval, fzval, pflval, pfaval, pfbval);
    return 0;
}


/*!
 * \brief   convertLABToRGB()
 *
 * \param[in]    flval, faval, fbval   input lab
 * \param[out]   prval, pgval, pbval   equivalent rgb values
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) For values of lab that are out of gamut for rgb, the rgb
 *          components are set to the closest valid color.
 * </pre>
 */
l_ok
convertLABToRGB(l_float32  flval,
                l_float32  faval,
                l_float32  fbval,
                l_int32   *prval,
                l_int32   *pgval,
                l_int32   *pbval)
{
l_float32  fxval, fyval, fzval;

    if (prval) *prval = 0;
    if (pgval) *pgval = 0;
    if (pbval) *pbval = 0;
    if (!prval || !pgval || !pbval)
        return ERROR_INT("&rval, &gval, &bval not all defined", __func__, 1);

    convertLABToXYZ(flval, faval, fbval, &fxval, &fyval, &fzval);
    convertXYZToRGB(fxval, fyval, fzval, 0, prval, pgval, pbval);
    return 0;
}


/*---------------------------------------------------------------------------*
 *                   Gamut display of RGB color space                        *
 *---------------------------------------------------------------------------*/
/*!
 * \brief   pixMakeGamutRGB()
 *
 * \param[in]    scale    default = 4
 * \return  pix2   32 bpp rgb
 *
 * <pre>
 * Notes:
 *      (1) This is an image that has all RGB colors, divided into 2^15
 *          cubical cells with 8x8x8 = 512 pixel values.  Each of the 32
 *          subimages has a constant value of B, with R and G varying over
 *          their gamut in 32 steps of size 8.
 *      (2) The %scale parameter determines the replication in both x and y
 *          of each of the 2^15 colors.  With a scale factor of 4, the
 *          output image has 4 * 4 * 2^15 = 0.5M pixels.
 *      (3) This useful for visualizing how filters, such as
 *          pixMakeArbMaskFromRGB(), separate colors into sets.
 * </pre>
 */
PIX *
pixMakeGamutRGB(l_int32 scale)
{
l_int32   i, j, k;
l_uint32  val32;
PIX      *pix1, *pix2;
PIXA     *pixa;

    if (scale <= 0) scale = 8;  /* default */

    pixa = pixaCreate(32);
    for (k = 0; k < 32; k++) {
        pix1 = pixCreate(32, 32, 32);
        for (i = 0; i < 32; i++) {
            for (j = 0; j < 32; j++) {
                composeRGBPixel(8 * j, 8 * i, 8 * k, &val32);
                pixSetPixel(pix1, j, i, val32);
            }
        }
        pixaAddPix(pixa, pix1, L_INSERT);
    }
    pix2 = pixaDisplayTiledInColumns(pixa, 8, scale, 5, 0);
    pixaDestroy(&pixa);
    return pix2;
}
