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
 * \file paintcmap.c
 * <pre>
 *
 *      These in-place functions paint onto colormap images.
 *
 *      Repaint selected pixels in region
 *           l_int32     pixSetSelectCmap()
 *
 *      Repaint non-white pixels in region
 *           l_int32     pixColorGrayRegionsCmap()
 *           l_int32     pixColorGrayCmap()
 *           l_int32     pixColorGrayMaskedCmap()
 *           l_int32     addColorizedGrayToCmap()
 *
 *      Repaint selected pixels through mask
 *           l_int32     pixSetSelectMaskedCmap()
 *
 *      Repaint all pixels through mask
 *           l_int32     pixSetMaskedCmap()
 *
 *
 *  The 'set select' functions condition the setting on a specific
 *  pixel value (i.e., index into the colormap) of the underlying
 *  Pix that is being modified.  The same conditioning is used in
 *  pixBlendCmap().
 *
 *  The pixColorGrayCmap() function sets all truly gray (r = g = b) pixels,
 *  with the exception of either black or white pixels, to a new color.
 *
 *  The pixSetSelectMaskedCmap() function conditions pixel painting
 *  on both a specific pixel value and location within the fg mask.
 *  By contrast, pixSetMaskedCmap() sets all pixels under the
 *  mask foreground, without considering the initial pixel values.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"

/*-------------------------------------------------------------*
 *               Repaint selected pixels in region             *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixSetSelectCmap()
 *
 * \param[in]    pixs              1, 2, 4 or 8 bpp, with colormap
 * \param[in]    box               [optional] region to set color; can be NULL
 * \param[in]    sindex            colormap index of pixels to be changed
 * \param[in]    rval, gval, bval  new color to paint
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is an in-place operation.
 *      (2) It sets all pixels in region that have the color specified
 *          by the colormap index %sindex to the new color.
 *      (3) %sindex must be in the existing colormap; otherwise an
 *          error is returned.
 *      (4) If the new color exists in the colormap, it is used;
 *          otherwise, it is added to the colormap.  If it cannot be
 *          added because the colormap is full, an error is returned.
 *      (5) If %box is NULL, applies function to the entire image; otherwise,
 *          clips the operation to the intersection of the box and pix.
 *      (6) An example of use would be to set to a specific color all
 *          the light (background) pixels within a certain region of
 *          a 3-level 2 bpp image, while leaving light pixels outside
 *          this region unchanged.
 * </pre>
 */
l_ok
pixSetSelectCmap(PIX     *pixs,
                 BOX     *box,
                 l_int32  sindex,
                 l_int32  rval,
                 l_int32  gval,
                 l_int32  bval)
{
l_int32    i, j, w, h, d, n, x1, y1, x2, y2, bw, bh, val, wpls;
l_int32    index;  /* of new color to be set */
l_uint32  *lines, *datas;
PIXCMAP   *cmap;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if ((cmap = pixGetColormap(pixs)) == NULL)
        return ERROR_INT("no colormap", __func__, 1);
    d = pixGetDepth(pixs);
    if (d != 1 && d != 2 && d != 4 && d != 8)
        return ERROR_INT("depth not in {1,2,4,8}", __func__, 1);

        /* Add new color if necessary; get index of this color in cmap */
    n = pixcmapGetCount(cmap);
    if (sindex >= n)
        return ERROR_INT("sindex too large; no cmap entry", __func__, 1);
    if (pixcmapGetIndex(cmap, rval, gval, bval, &index)) { /* not found */
        if (pixcmapAddColor(cmap, rval, gval, bval))
            return ERROR_INT("error adding cmap entry", __func__, 1);
        else
            index = n;  /* we've added one color */
    }

        /* Determine the region of substitution */
    pixGetDimensions(pixs, &w, &h, NULL);
    if (!box) {
        x1 = y1 = 0;
        x2 = w;
        y2 = h;
    } else {
        boxGetGeometry(box, &x1, &y1, &bw, &bh);
        x2 = x1 + bw - 1;
        y2 = y1 + bh - 1;
    }

        /* Replace pixel value sindex by index in the region */
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    for (i = y1; i <= y2; i++) {
        if (i < 0 || i >= h)  /* clip */
            continue;
        lines = datas + i * wpls;
        for (j = x1; j <= x2; j++) {
            if (j < 0 || j >= w)  /* clip */
                continue;
            switch (d) {
            case 1:
                val = GET_DATA_BIT(lines, j);
                if (val == sindex) {
                    if (index == 0)
                        CLEAR_DATA_BIT(lines, j);
                    else
                        SET_DATA_BIT(lines, j);
                }
                break;
            case 2:
                val = GET_DATA_DIBIT(lines, j);
                if (val == sindex)
                    SET_DATA_DIBIT(lines, j, index);
                break;
            case 4:
                val = GET_DATA_QBIT(lines, j);
                if (val == sindex)
                    SET_DATA_QBIT(lines, j, index);
                break;
            case 8:
                val = GET_DATA_BYTE(lines, j);
                if (val == sindex)
                    SET_DATA_BYTE(lines, j, index);
                break;
            default:
                return ERROR_INT("depth not in {1,2,4,8}", __func__, 1);
            }
        }
    }

    return 0;
}


/*-------------------------------------------------------------*
 *                  Repaint gray pixels in region              *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixColorGrayRegionsCmap()
 *
 * \param[in]    pixs               8 bpp, with colormap
 * \param[in]    boxa               of regions in which to apply color
 * \param[in]    type               L_PAINT_LIGHT, L_PAINT_DARK
 * \param[in]    rval, gval, bval   target color
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is an in-place operation.
 *      (2) If %type == L_PAINT_LIGHT, it colorizes non-black pixels,
 *          preserving antialiasing.
 *          If %type == L_PAINT_DARK, it colorizes non-white pixels,
 *          preserving antialiasing.  See pixColorGrayCmap() for details.
 *      (3) This can also be called through pixColorGrayRegions().
 *      (4) This increases the colormap size by the number of
 *          different gray (non-black or non-white) colors in the
 *          selected regions of pixs.  If there is not enough room in
 *          the colormap for this expansion, it returns 1 (error),
 *          and the caller should check the return value.
 *      (5) Because two boxes in %boxa can overlap, pixels that
 *          are colorized in the first box must be excluded in the
 *          second because their value exceeds the size of the map.
 * </pre>
 */
l_ok
pixColorGrayRegionsCmap(PIX     *pixs,
                        BOXA    *boxa,
                        l_int32  type,
                        l_int32  rval,
                        l_int32  gval,
                        l_int32  bval)
{
l_int32    i, j, k, w, h, n, nc, x1, y1, x2, y2, bw, bh, wpl;
l_int32    val, nval;
l_int32   *map;
l_uint32  *line, *data;
BOX       *box;
NUMA      *na;
PIXCMAP   *cmap;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);
    if ((cmap = pixGetColormap(pixs)) == NULL)
        return ERROR_INT("no colormap", __func__, 1);
    if (pixGetDepth(pixs) != 8)
        return ERROR_INT("depth not 8 bpp", __func__, 1);
    if (type != L_PAINT_DARK && type != L_PAINT_LIGHT)
        return ERROR_INT("invalid type", __func__, 1);

    nc = pixcmapGetCount(cmap);
    if (addColorizedGrayToCmap(cmap, type, rval, gval, bval, &na))
        return ERROR_INT("no room; cmap full", __func__, 1);
    map = numaGetIArray(na);
    numaDestroy(&na);
    if (!map)
        return ERROR_INT("map not made", __func__, 1);

    pixGetDimensions(pixs, &w, &h, NULL);
    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    n = boxaGetCount(boxa);
    for (k = 0; k < n; k++) {
        box = boxaGetBox(boxa, k, L_CLONE);
        boxGetGeometry(box, &x1, &y1, &bw, &bh);
        x2 = x1 + bw - 1;
        y2 = y1 + bh - 1;

            /* Remap gray pixels in the region */
        for (i = y1; i <= y2; i++) {
            if (i < 0 || i >= h)  /* clip */
                continue;
            line = data + i * wpl;
            for (j = x1; j <= x2; j++) {
                if (j < 0 || j >= w)  /* clip */
                    continue;
                val = GET_DATA_BYTE(line, j);
                if (val >= nc) continue;  /* from overlapping b.b. */
                nval = map[val];
                if (nval != 256)
                    SET_DATA_BYTE(line, j, nval);
            }
        }
        boxDestroy(&box);
    }

    LEPT_FREE(map);
    return 0;
}


/*!
 * \brief   pixColorGrayCmap()
 *
 * \param[in]    pixs               2, 4 or 8 bpp, with colormap
 * \param[in]    box                [optional] region to set color; can be NULL
 * \param[in]    type               L_PAINT_LIGHT, L_PAINT_DARK
 * \param[in]    rval, gval, bval   target color
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is an in-place operation.
 *      (2) If %type == L_PAINT_LIGHT, it colorizes non-black pixels,
 *          preserving antialiasing.
 *          If %type == L_PAINT_DARK, it colorizes non-white pixels,
 *          preserving antialiasing.
 *      (3) %box gives the region to apply color; if NULL, this
 *          colorizes the entire image.
 *      (4) If the cmap is only 2 or 4 bpp, pixs is converted in-place
 *          to an 8 bpp cmap.  A 1 bpp cmap is not a valid input pix.
 *      (5) This can also be called through pixColorGray().
 *      (6) This operation increases the colormap size by the number of
 *          different gray (non-black or non-white) colors in the
 *          input colormap.  If there is not enough room in the colormap
 *          for this expansion, it returns 1 (error), and the caller
 *          should check the return value.
 *      (7) Using the darkness of each original pixel in the rect,
 *          it generates a new color (based on the input rgb values).
 *          If %type == L_PAINT_LIGHT, the new color is a (generally)
 *          darken-to-black version of the input rgb color, where the
 *          amount of darkening increases with the darkness of the
 *          original pixel color.
 *          If %type == L_PAINT_DARK, the new color is a (generally)
 *          faded-to-white version of the input rgb color, where the
 *          amount of fading increases with the brightness of the
 *          original pixel color.
 * </pre>
 */
l_ok
pixColorGrayCmap(PIX     *pixs,
                 BOX     *box,
                 l_int32  type,
                 l_int32  rval,
                 l_int32  gval,
                 l_int32  bval)
{
l_int32   w, h, d, ret;
PIX      *pixt;
BOXA     *boxa;
PIXCMAP  *cmap;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if ((cmap = pixGetColormap(pixs)) == NULL)
        return ERROR_INT("no colormap", __func__, 1);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 2 && d != 4 && d != 8)
        return ERROR_INT("depth not in {2, 4, 8}", __func__, 1);
    if (type != L_PAINT_DARK && type != L_PAINT_LIGHT)
        return ERROR_INT("invalid type", __func__, 1);

        /* If 2 bpp or 4 bpp, convert in-place to 8 bpp. */
    if (d == 2 || d == 4) {
        pixt = pixConvertTo8(pixs, 1);
        pixTransferAllData(pixs, &pixt, 0, 0);
    }

        /* If box == NULL, color the entire image */
    boxa = boxaCreate(1);
    if (box) {
        boxaAddBox(boxa, box, L_COPY);
    } else {
        box = boxCreate(0, 0, w, h);
        boxaAddBox(boxa, box, L_INSERT);
    }
    ret = pixColorGrayRegionsCmap(pixs, boxa, type, rval, gval, bval);

    boxaDestroy(&boxa);
    return ret;
}


/*!
 * \brief   pixColorGrayMaskedCmap()
 *
 * \param[in]    pixs               8 bpp, with colormap
 * \param[in]    pixm               1 bpp mask, through which to apply color
 * \param[in]    type               L_PAINT_LIGHT, L_PAINT_DARK
 * \param[in]    rval, gval, bval   target color
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is an in-place operation.
 *      (2) If %type == L_PAINT_LIGHT, it colorizes non-black pixels,
 *          preserving antialiasing.
 *          If %type == L_PAINT_DARK, it colorizes non-white pixels,
 *          preserving antialiasing.  See pixColorGrayCmap() for details.
 *      (3) This increases the colormap size by the number of
 *          different gray (non-black or non-white) colors in the
 *          input colormap.  If there is not enough room in the colormap
 *          for this expansion, it returns 1 (error).
 * </pre>
 */
l_ok
pixColorGrayMaskedCmap(PIX     *pixs,
                       PIX     *pixm,
                       l_int32  type,
                       l_int32  rval,
                       l_int32  gval,
                       l_int32  bval)
{
l_int32    i, j, w, h, wm, hm, wmin, hmin, wpl, wplm;
l_int32    val, nval;
l_int32   *map;
l_uint32  *line, *data, *linem, *datam;
NUMA      *na;
PIXCMAP   *cmap;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (!pixm || pixGetDepth(pixm) != 1)
        return ERROR_INT("pixm undefined or not 1 bpp", __func__, 1);
    if ((cmap = pixGetColormap(pixs)) == NULL)
        return ERROR_INT("no colormap", __func__, 1);
    if (pixGetDepth(pixs) != 8)
        return ERROR_INT("depth not 8 bpp", __func__, 1);
    if (type != L_PAINT_DARK && type != L_PAINT_LIGHT)
        return ERROR_INT("invalid type", __func__, 1);

    if (addColorizedGrayToCmap(cmap, type, rval, gval, bval, &na))
        return ERROR_INT("no room; cmap full", __func__, 1);
    map = numaGetIArray(na);
    numaDestroy(&na);
    if (!map)
        return ERROR_INT("map not made", __func__, 1);

    pixGetDimensions(pixs, &w, &h, NULL);
    pixGetDimensions(pixm, &wm, &hm, NULL);
    if (wm != w)
        L_WARNING("wm = %d differs from w = %d\n", __func__, wm, w);
    if (hm != h)
        L_WARNING("hm = %d differs from h = %d\n", __func__, hm, h);
    wmin = L_MIN(w, wm);
    hmin = L_MIN(h, hm);

    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    datam = pixGetData(pixm);
    wplm = pixGetWpl(pixm);

        /* Remap gray pixels in the region */
    for (i = 0; i < hmin; i++) {
        line = data + i * wpl;
        linem = datam + i * wplm;
        for (j = 0; j < wmin; j++) {
            if (GET_DATA_BIT(linem, j) == 0)
                continue;
            val = GET_DATA_BYTE(line, j);
            nval = map[val];
            if (nval != 256)
                SET_DATA_BYTE(line, j, nval);
        }
    }

    LEPT_FREE(map);
    return 0;
}


/*!
 * \brief   addColorizedGrayToCmap()
 *
 * \param[in]    cmap              from 2 or 4 bpp pix
 * \param[in]    type              L_PAINT_LIGHT, L_PAINT_DARK
 * \param[in]    rval, gval, bval  target color
 * \param[out]   pna               [optional] table for mapping new cmap entries
 * \return  0 if OK; 1 on error; 2 if new colors will not fit in cmap.
 *
 * <pre>
 * Notes:
 *      (1) If %type == L_PAINT_LIGHT, it colorizes non-black pixels,
 *          preserving antialiasing.
 *          If %type == L_PAINT_DARK, it colorizes non-white pixels,
 *          preserving antialiasing.
 *      (2) This increases the colormap size by the number of
 *          different gray (non-black or non-white) colors in the
 *          input colormap.  If there is not enough room in the colormap
 *          for this expansion, it returns 1 (treated as a warning);
 *          the caller should check the return value.
 *      (3) This can be used to determine if the new colors will fit in
 *          the cmap, using null for &na.  Returns 0 if they fit; 2 if
 *          they don't fit.
 *      (4) The mapping table contains, for each gray color found, the
 *          index of the corresponding colorized pixel.  Non-gray
 *          pixels are assigned the invalid index 256.
 *      (5) See pixColorGrayCmap() for usage.
 * </pre>
 */
l_ok
addColorizedGrayToCmap(PIXCMAP  *cmap,
                       l_int32   type,
                       l_int32   rval,
                       l_int32   gval,
                       l_int32   bval,
                       NUMA    **pna)
{
l_int32  i, n, erval, egval, ebval, nrval, ngval, nbval, newindex;
NUMA    *na;

    if (pna) *pna = NULL;
    if (!cmap)
        return ERROR_INT("cmap not defined", __func__, 1);
    if (type != L_PAINT_DARK && type != L_PAINT_LIGHT)
        return ERROR_INT("invalid type", __func__, 1);

    n = pixcmapGetCount(cmap);
    na = numaCreate(n);
    for (i = 0; i < n; i++) {
        pixcmapGetColor(cmap, i, &erval, &egval, &ebval);
        if (type == L_PAINT_LIGHT) {
            if (erval == egval && erval == ebval && erval != 0) {
                nrval = (l_int32)(rval * (l_float32)erval / 255.);
                ngval = (l_int32)(gval * (l_float32)egval / 255.);
                nbval = (l_int32)(bval * (l_float32)ebval / 255.);
                if (pixcmapAddNewColor(cmap, nrval, ngval, nbval, &newindex)) {
                    numaDestroy(&na);
                    L_WARNING("no room; colormap full\n", __func__);
                    return 2;
                }
                numaAddNumber(na, newindex);
            } else {
                numaAddNumber(na, 256);  /* invalid number; not gray */
            }
        } else {  /* L_PAINT_DARK */
            if (erval == egval && erval == ebval && erval != 255) {
                nrval = rval +
                        (l_int32)((255. - rval) * (l_float32)erval / 255.);
                ngval = gval +
                        (l_int32)((255. - gval) * (l_float32)egval / 255.);
                nbval = bval +
                        (l_int32)((255. - bval) * (l_float32)ebval / 255.);
                if (pixcmapAddNewColor(cmap, nrval, ngval, nbval, &newindex)) {
                    numaDestroy(&na);
                    L_WARNING("no room; colormap full\n", __func__);
                    return 2;
                }
                numaAddNumber(na, newindex);
            } else {
                numaAddNumber(na, 256);  /* invalid number; not gray */
            }
        }
    }

    if (pna)
        *pna = na;
    else
        numaDestroy(&na);
    return 0;
}


/*-------------------------------------------------------------*
 *             Repaint selected pixels through mask            *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixSetSelectMaskedCmap()
 *
 * \param[in]    pixs               2, 4 or 8 bpp, with colormap
 * \param[in]    pixm               [optional] 1 bpp mask; no-op if NULL
 * \param[in]    x, y               UL corner of mask relative to pixs
 * \param[in]    sindex             cmap index of pixels in pixs to be changed
 * \param[in]    rval, gval, bval   new color to substitute
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is an in-place operation.
 *      (2) This paints through the fg of pixm and replaces all pixels
 *          in pixs that have the value %sindex with the new color.
 *      (3) If pixm == NULL, a warning is given.
 *      (4) %sindex must be in the existing colormap; otherwise an
 *          error is returned.
 *      (5) If the new color exists in the colormap, it is used;
 *          otherwise, it is added to the colormap.  If the colormap
 *          is full, an error is returned.
 * </pre>
 */
l_ok
pixSetSelectMaskedCmap(PIX     *pixs,
                       PIX     *pixm,
                       l_int32  x,
                       l_int32  y,
                       l_int32  sindex,
                       l_int32  rval,
                       l_int32  gval,
                       l_int32  bval)
{
l_int32    i, j, w, h, d, n, wm, hm, wpls, wplm, val;
l_int32    index;  /* of new color to be set */
l_uint32  *lines, *linem, *datas, *datam;
PIXCMAP   *cmap;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if ((cmap = pixGetColormap(pixs)) == NULL)
        return ERROR_INT("no colormap", __func__, 1);
    if (!pixm) {
        L_WARNING("no mask; nothing to do\n", __func__);
        return 0;
    }

    d = pixGetDepth(pixs);
    if (d != 2 && d != 4 && d != 8)
        return ERROR_INT("depth not in {2, 4, 8}", __func__, 1);

        /* add new color if necessary; get index of this color in cmap */
    n = pixcmapGetCount(cmap);
    if (sindex >= n)
        return ERROR_INT("sindex too large; no cmap entry", __func__, 1);
    if (pixcmapGetIndex(cmap, rval, gval, bval, &index)) { /* not found */
        if (pixcmapAddColor(cmap, rval, gval, bval))
            return ERROR_INT("error adding cmap entry", __func__, 1);
        else
            index = n;  /* we've added one color */
    }

        /* replace pixel value sindex by index when fg pixel in pixmc
         * overlays it */
    pixGetDimensions(pixs, &w, &h, NULL);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    wm = pixGetWidth(pixm);
    hm = pixGetHeight(pixm);
    datam = pixGetData(pixm);
    wplm = pixGetWpl(pixm);
    for (i = 0; i < hm; i++) {
        if (i + y < 0 || i + y >= h) continue;
        lines = datas + (y + i) * wpls;
        linem = datam + i * wplm;
        for (j = 0; j < wm; j++) {
            if (j + x < 0  || j + x >= w) continue;
            if (GET_DATA_BIT(linem, j)) {
                switch (d) {
                case 2:
                    val = GET_DATA_DIBIT(lines, x + j);
                    if (val == sindex)
                        SET_DATA_DIBIT(lines, x + j, index);
                    break;
                case 4:
                    val = GET_DATA_QBIT(lines, x + j);
                    if (val == sindex)
                        SET_DATA_QBIT(lines, x + j, index);
                    break;
                case 8:
                    val = GET_DATA_BYTE(lines, x + j);
                    if (val == sindex)
                        SET_DATA_BYTE(lines, x + j, index);
                    break;
                default:
                    return ERROR_INT("depth not in {1,2,4,8}", __func__, 1);
                }
            }
        }
    }

    return 0;
}


/*-------------------------------------------------------------*
 *               Repaint all pixels through mask               *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixSetMaskedCmap()
 *
 * \param[in]    pixs               2, 4 or 8 bpp, colormapped
 * \param[in]    pixm               [optional] 1 bpp mask; no-op if NULL
 * \param[in]    x, y               origin of pixm relative to pixs;
 *                                  can be negative
 * \param[in]    rval, gval, bval   new color to set at each masked pixel
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is an in-place operation.
 *      (2) It paints a single color through the mask (as a stencil).
 *      (3) The mask origin is placed at (%x,%y) on %pixs, and the
 *          operation is clipped to the intersection of the mask and pixs.
 *      (4) If %pixm == NULL, a warning is given.
 *      (5) Typically, %pixm is a small binary mask located somewhere
 *          on the larger %pixs.
 *      (6) If the color is in the colormap, it is used.  Otherwise,
 *          it is added if possible; an error is returned if the
 *          colormap is already full.
 * </pre>
 */
l_ok
pixSetMaskedCmap(PIX      *pixs,
                 PIX      *pixm,
                 l_int32   x,
                 l_int32   y,
                 l_int32   rval,
                 l_int32   gval,
                 l_int32   bval)
{
l_int32    w, h, d, wpl, wm, hm, wplm;
l_int32    i, j, index;
l_uint32  *data, *datam, *line, *linem;
PIXCMAP   *cmap;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if ((cmap = pixGetColormap(pixs)) == NULL)
        return ERROR_INT("no colormap in pixs", __func__, 1);
    if (!pixm) {
        L_WARNING("no mask; nothing to do\n", __func__);
        return 0;
    }
    d = pixGetDepth(pixs);
    if (d != 2 && d != 4 && d != 8)
        return ERROR_INT("depth not in {2,4,8}", __func__, 1);
    if (pixGetDepth(pixm) != 1)
        return ERROR_INT("pixm not 1 bpp", __func__, 1);

        /* Add new color if necessary; store in 'index' */
    if (pixcmapGetIndex(cmap, rval, gval, bval, &index)) {  /* not found */
        if (pixcmapAddColor(cmap, rval, gval, bval))
            return ERROR_INT("no room in cmap", __func__, 1);
        index = pixcmapGetCount(cmap) - 1;
    }

    pixGetDimensions(pixs, &w, &h, NULL);
    wpl = pixGetWpl(pixs);
    data = pixGetData(pixs);
    pixGetDimensions(pixm, &wm, &hm, NULL);
    wplm = pixGetWpl(pixm);
    datam = pixGetData(pixm);
    for (i = 0; i < hm; i++) {
        if (i + y < 0 || i + y >= h) continue;
        line = data + (i + y) * wpl;
        linem = datam + i * wplm;
        for (j = 0; j < wm; j++) {
            if (j + x < 0  || j + x >= w) continue;
            if (GET_DATA_BIT(linem, j)) {  /* paint color */
                switch (d) {
                case 2:
                    SET_DATA_DIBIT(line, j + x, index);
                    break;
                case 4:
                    SET_DATA_QBIT(line, j + x, index);
                    break;
                case 8:
                    SET_DATA_BYTE(line, j + x, index);
                    break;
                default:
                    return ERROR_INT("depth not in {2,4,8}", __func__, 1);
                }
            }
        }
    }

    return 0;
}
