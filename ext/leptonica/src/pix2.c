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
 * \file pix2.c
 * <pre>
 *
 *    This file has these basic operations:
 *
 *      (1) Get and set: individual pixels, full image, rectangular region,
 *          pad pixels, border pixels, and color components for RGB
 *      (2) Add and remove border pixels
 *      (3) Endian byte swaps
 *      (4) Simple method for byte-processing images (instead of words)
 *
 *      Pixel poking
 *           l_int32     pixGetPixel()
 *           l_int32     pixSetPixel()
 *           l_int32     pixGetRGBPixel()
 *           l_int32     pixSetRGBPixel()
 *           l_int32     pixSetCmapPixel()
 *           l_int32     pixGetRandomPixel()
 *           l_int32     pixClearPixel()
 *           l_int32     pixFlipPixel()
 *           void        setPixelLow()
 *
 *      Find black or white value
 *           l_int32     pixGetBlackOrWhiteVal()
 *
 *      Full image clear/set/set-to-arbitrary-value
 *           l_int32     pixClearAll()
 *           l_int32     pixSetAll()
 *           l_int32     pixSetAllGray()
 *           l_int32     pixSetAllArbitrary()
 *           l_int32     pixSetBlackOrWhite()
 *           l_int32     pixSetComponentArbitrary()
 *
 *      Rectangular region clear/set/set-to-arbitrary-value/blend
 *           l_int32     pixClearInRect()
 *           l_int32     pixSetInRect()
 *           l_int32     pixSetInRectArbitrary()
 *           l_int32     pixBlendInRect()
 *
 *      Set pad bits
 *           l_int32     pixSetPadBits()
 *           l_int32     pixSetPadBitsBand()
 *
 *      Assign border pixels
 *           l_int32     pixSetOrClearBorder()
 *           l_int32     pixSetBorderVal()
 *           l_int32     pixSetBorderRingVal()
 *           l_int32     pixSetMirroredBorder()
 *           PIX        *pixCopyBorder()
 *
 *      Add and remove border
 *           PIX        *pixAddBorder()
 *           PIX        *pixAddBlackOrWhiteBorder()
 *           PIX        *pixAddBorderGeneral()
 *           PIX        *pixAddMultipleBlackWhiteBorders()
 *           PIX        *pixRemoveBorder()
 *           PIX        *pixRemoveBorderGeneral()
 *           PIX        *pixRemoveBorderToSize()
 *           PIX        *pixAddMirroredBorder()
 *           PIX        *pixAddRepeatedBorder()
 *           PIX        *pixAddMixedBorder()
 *           PIX        *pixAddContinuedBorder()
 *
 *      Helper functions using alpha
 *           l_int32     pixShiftAndTransferAlpha()
 *           PIX        *pixDisplayLayersRGBA()
 *
 *      Color sample setting and extraction
 *           PIX        *pixCreateRGBImage()
 *           PIX        *pixGetRGBComponent()
 *           l_int32     pixSetRGBComponent()
 *           PIX        *pixGetRGBComponentCmap()
 *           l_int32     pixCopyRGBComponent()
 *           l_int32     composeRGBPixel()
 *           l_int32     composeRGBAPixel()
 *           void        extractRGBValues()
 *           void        extractRGBAValues()
 *           l_int32     extractMinMaxComponent()
 *           l_int32     pixGetRGBLine()
 *
 *      Raster line pixel setter
 *           l_int32     setLineDataVal()
 *
 *      Conversion between big and little endians
 *           PIX        *pixEndianByteSwapNew()
 *           l_int32     pixEndianByteSwap()
 *           l_int32     lineEndianByteSwap()
 *           PIX        *pixEndianTwoByteSwapNew()
 *           l_int32     pixEndianTwoByteSwap()
 *
 *      Extract raster data as binary string
 *           l_int32     pixGetRasterData()
 *
 *      Test alpha component opaqueness
 *           l_int32     pixAlphaIsOpaque()
 *
 *      Infer resolution from image size
 *           l_int32     pixInferResolution()
 *
 *      Setup helpers for 8 bpp byte processing
 *           l_uint8   **pixSetupByteProcessing()
 *           l_int32     pixCleanupByteProcessing()
 *
 *      Setting parameters for antialias masking with alpha transforms
 *           void        l_setAlphaMaskBorder()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"
#include "pix_internal.h"

static const l_uint32 rmask32[] = {0x0,
    0x00000001, 0x00000003, 0x00000007, 0x0000000f,
    0x0000001f, 0x0000003f, 0x0000007f, 0x000000ff,
    0x000001ff, 0x000003ff, 0x000007ff, 0x00000fff,
    0x00001fff, 0x00003fff, 0x00007fff, 0x0000ffff,
    0x0001ffff, 0x0003ffff, 0x0007ffff, 0x000fffff,
    0x001fffff, 0x003fffff, 0x007fffff, 0x00ffffff,
    0x01ffffff, 0x03ffffff, 0x07ffffff, 0x0fffffff,
    0x1fffffff, 0x3fffffff, 0x7fffffff, 0xffffffff};

    /* This is a global that determines the default 8 bpp alpha mask values
     * for rings at distance 1 and 2 from the border.  Declare extern
     * to use.  To change the values, use l_setAlphaMaskBorder(). */
LEPT_DLL l_float32  AlphaMaskBorderVals[2] = {0.0, 0.5};


#ifndef  NO_CONSOLE_IO
#define  DEBUG_SERIALIZE        0
#endif  /* ~NO_CONSOLE_IO */


/*-------------------------------------------------------------*
 *                         Pixel poking                        *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixGetPixel()
 *
 * \param[in]    pix
 * \param[in]    x,y    pixel coords
 * \param[out]   pval   pixel value
 * \return  0 if OK; 1 or 2 on error
 *
 * <pre>
 * Notes:
 *      (1) This returns the value in the data array.  If the pix is
 *          colormapped, it returns the colormap index, not the rgb value.
 *      (2) Because of the function overhead and the parameter checking,
 *          this is much slower than using the GET_DATA_*() macros directly.
 *          Speed on a 1 Mpixel RGB image, using a 3 GHz machine:
 *            * pixGet/pixSet: ~25 Mpix/sec
 *            * GET_DATA/SET_DATA: ~350 MPix/sec
 *          If speed is important and you're doing random access into
 *          the pix, use pixGetLinePtrs() and the array access macros.
 *      (3) If the point is outside the image, this returns an error (2),
 *          with 0 in %pval.  To avoid spamming output, it fails silently.
 * </pre>
 */
l_ok
pixGetPixel(PIX       *pix,
            l_int32    x,
            l_int32    y,
            l_uint32  *pval)
{
l_int32    w, h, d, wpl, val;
l_uint32  *line, *data;

    if (!pval)
        return ERROR_INT("&val not defined", __func__, 1);
    *pval = 0;
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    pixGetDimensions(pix, &w, &h, &d);
    if (x < 0 || x >= w || y < 0 || y >= h)
        return 2;

    wpl = pixGetWpl(pix);
    data = pixGetData(pix);
    line = data + y * wpl;
    switch (d)
    {
    case 1:
        val = GET_DATA_BIT(line, x);
        break;
    case 2:
        val = GET_DATA_DIBIT(line, x);
        break;
    case 4:
        val = GET_DATA_QBIT(line, x);
        break;
    case 8:
        val = GET_DATA_BYTE(line, x);
        break;
    case 16:
        val = GET_DATA_TWO_BYTES(line, x);
        break;
    case 32:
        val = line[x];
        break;
    default:
        return ERROR_INT("depth must be in {1,2,4,8,16,32} bpp", __func__, 1);
    }

    *pval = val;
    return 0;
}


/*!
 * \brief   pixSetPixel()
 *
 * \param[in]    pix
 * \param[in]    x,y   pixel coords
 * \param[in]    val   value to be inserted
 * \return  0 if OK; 1 or 2 on error
 *
 * <pre>
 * Notes:
 *      (1) Warning: the input value is not checked for overflow with respect
 *          the the depth of %pix, and the sign bit (if any) is ignored.
 *          * For d == 1, %val > 0 sets the bit on.
 *          * For d == 2, 4, 8 and 16, %val is masked to the maximum allowable
 *            pixel value, and any (invalid) higher order bits are discarded.
 *      (2) See pixGetPixel() for information on performance.
 *      (3) If the point is outside the image, this returns an error (2),
 *          with 0 in %pval.  To avoid spamming output, it fails silently.
 * </pre>
 */
l_ok
pixSetPixel(PIX      *pix,
            l_int32   x,
            l_int32   y,
            l_uint32  val)
{
l_int32    w, h, d, wpl;
l_uint32  *line, *data;

    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    pixGetDimensions(pix, &w, &h, &d);
    if (x < 0 || x >= w || y < 0 || y >= h)
        return 2;

    data = pixGetData(pix);
    wpl = pixGetWpl(pix);
    line = data + y * wpl;
    switch (d)
    {
    case 1:
        if (val)
            SET_DATA_BIT(line, x);
        else
            CLEAR_DATA_BIT(line, x);
        break;
    case 2:
        SET_DATA_DIBIT(line, x, val);
        break;
    case 4:
        SET_DATA_QBIT(line, x, val);
        break;
    case 8:
        SET_DATA_BYTE(line, x, val);
        break;
    case 16:
        SET_DATA_TWO_BYTES(line, x, val);
        break;
    case 32:
        line[x] = val;
        break;
    default:
        return ERROR_INT("depth must be in {1,2,4,8,16,32} bpp", __func__, 1);
    }

    return 0;
}


/*!
 * \brief   pixGetRGBPixel()
 *
 * \param[in]    pix    32 bpp rgb, not colormapped
 * \param[in]    x,y    pixel coords
 * \param[out]   prval  [optional] red component
 * \param[out]   pgval  [optional] green component
 * \param[out]   pbval  [optional] blue component
 * \return  0 if OK; 1 or 2 on error
 *
 * <pre>
 * Notes:
 *      (1) If the point is outside the image, this returns an error (2),
 *          with 0 in %pval.  To avoid spamming output, it fails silently.
 * </pre>
 */
l_ok
pixGetRGBPixel(PIX      *pix,
               l_int32   x,
               l_int32   y,
               l_int32  *prval,
               l_int32  *pgval,
               l_int32  *pbval)
{
l_int32    w, h, d, wpl;
l_uint32  *data, *ppixel;

    if (prval) *prval = 0;
    if (pgval) *pgval = 0;
    if (pbval) *pbval = 0;
    if (!prval && !pgval && !pbval)
        return ERROR_INT("no output requested", __func__, 1);
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    pixGetDimensions(pix, &w, &h, &d);
    if (d != 32)
        return ERROR_INT("pix not 32 bpp", __func__, 1);
    if (x < 0 || x >= w || y < 0 || y >= h)
        return 2;

    wpl = pixGetWpl(pix);
    data = pixGetData(pix);
    ppixel = data + y * wpl + x;
    if (prval) *prval = GET_DATA_BYTE(ppixel, COLOR_RED);
    if (pgval) *pgval = GET_DATA_BYTE(ppixel, COLOR_GREEN);
    if (pbval) *pbval = GET_DATA_BYTE(ppixel, COLOR_BLUE);
    return 0;
}


/*!
 * \brief   pixSetRGBPixel()
 *
 * \param[in]    pix    32 bpp rgb
 * \param[in]    x,y    pixel coords
 * \param[in]    rval   red component
 * \param[in]    gval   green component
 * \param[in]    bval   blue component
 * \return  0 if OK; 1 or 2 on error
 *
 * <pre>
 * Notes:
 *      (1) If the point is outside the image, this returns an error (2),
 *          and to avoid spamming output, it fails silently.
 * </pre>
 */
l_ok
pixSetRGBPixel(PIX     *pix,
               l_int32  x,
               l_int32  y,
               l_int32  rval,
               l_int32  gval,
               l_int32  bval)
{
l_int32    w, h, d, wpl;
l_uint32   pixel;
l_uint32  *data, *line;

    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    pixGetDimensions(pix, &w, &h, &d);
    if (d != 32)
        return ERROR_INT("pix not 32 bpp", __func__, 1);
    if (x < 0 || x >= w || y < 0 || y >= h)
        return 2;

    wpl = pixGetWpl(pix);
    data = pixGetData(pix);
    line = data + y * wpl;
    composeRGBPixel(rval, gval, bval, &pixel);
    *(line + x) = pixel;
    return 0;
}


/*!
 * \brief   pixSetCmapPixel()
 *
 * \param[in]    pix    2, 4 or 8 bpp, colormapped
 * \param[in]    x,y    pixel coords
 * \param[in]    rval   red component
 * \param[in]    gval   green component
 * \param[in]    bval   blue component
 * \return  0 if OK; 1 or 2 on error
 *
 * <pre>
 * Notes:
 *      (1) If the point is outside the image, this returns an error (2),
 *          and to avoid spamming output, it fails silently.
 *      (2) - If the color already exists, use it.
 *          - If the color does not exist in the colormap, it is added
 *            if possible.
 *          - If there is not room in the colormap for the new color:
 *            * if d < 8, return 2 with a warning.
 *            * if d == 8, find and use the nearest color.
 *      (3) Note that this operation scales with the number of colors
 *          in the colormap, and therefore can be very expensive if an
 *          attempt is made to set many pixels.  (In that case, it should
 *          be implemented with a map:rgb-->index for efficiency.)
 *          This is best used with very small images.
 * </pre>
 */
l_ok
pixSetCmapPixel(PIX     *pix,
                l_int32  x,
                l_int32  y,
                l_int32  rval,
                l_int32  gval,
                l_int32  bval)
{
l_int32   w, h, d, index;
PIXCMAP  *cmap;

    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    if ((cmap = pixGetColormap(pix)) == NULL)
        return ERROR_INT("pix is not colormapped", __func__, 1);
    pixGetDimensions(pix, &w, &h, &d);
    if (d != 2 && d != 4 && d != 8)
        return ERROR_INT("pix depth not 2, 4 or 8", __func__, 1);
    if (x < 0 || x >= w || y < 0 || y >= h)
        return 2;

    if (d == 8) {  /* always add */
        pixcmapAddNearestColor(cmap, rval, gval, bval, &index);
    } else {  /* d < 8 */
        if (pixcmapAddNewColor(cmap, rval, gval, bval, &index) == 2)
            return ERROR_INT("colormap is full", __func__, 2);
    }
    pixSetPixel(pix, x, y, index);
    return 0;
}


/*!
 * \brief   pixGetRandomPixel()
 *
 * \param[in]    pix    any depth; can be colormapped
 * \param[out]   pval   [optional] pixel value
 * \param[out]   px     [optional] x coordinate chosen; can be null
 * \param[out]   py     [optional] y coordinate chosen; can be null
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If the pix is colormapped, it returns the rgb value.
 * </pre>
 */
l_ok
pixGetRandomPixel(PIX       *pix,
                  l_uint32  *pval,
                  l_int32   *px,
                  l_int32   *py)
{
l_int32   w, h, x, y, rval, gval, bval;
l_uint32  val;
PIXCMAP  *cmap;

    if (pval) *pval = 0;
    if (px) *px = 0;
    if (py) *py = 0;
    if (!pval && !px && !py)
        return ERROR_INT("no output requested", __func__, 1);
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    pixGetDimensions(pix, &w, &h, NULL);
    x = rand() % w;
    y = rand() % h;
    if (px) *px = x;
    if (py) *py = y;
    if (pval) {
        pixGetPixel(pix, x, y, &val);
        if ((cmap = pixGetColormap(pix)) != NULL) {
            pixcmapGetColor(cmap, val, &rval, &gval, &bval);
            composeRGBPixel(rval, gval, bval, pval);
        } else {
            *pval = val;
        }
    }

    return 0;
}


/*!
 * \brief   pixClearPixel()
 *
 * \param[in]    pix   any depth; warning if colormapped
 * \param[in]    x,y   pixel coords
 * \return  0 if OK; 1 or 2 on error.
 *
 * <pre>
 * Notes:
 *      (1) If the point is outside the image, this returns an error (2),
 *          with 0 in %pval.  To avoid spamming output, it fails silently.
 * </pre>
 */
l_ok
pixClearPixel(PIX     *pix,
              l_int32  x,
              l_int32  y)
{
l_int32    w, h, d, wpl;
l_uint32  *line, *data;

    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    if (pixGetColormap(pix))
        L_WARNING("cmapped: setting to 0 may not be intended\n", __func__);
    pixGetDimensions(pix, &w, &h, &d);
    if (x < 0 || x >= w || y < 0 || y >= h)
        return 2;

    wpl = pixGetWpl(pix);
    data = pixGetData(pix);
    line = data + y * wpl;
    switch (d)
    {
    case 1:
        CLEAR_DATA_BIT(line, x);
        break;
    case 2:
        CLEAR_DATA_DIBIT(line, x);
        break;
    case 4:
        CLEAR_DATA_QBIT(line, x);
        break;
    case 8:
        SET_DATA_BYTE(line, x, 0);
        break;
    case 16:
        SET_DATA_TWO_BYTES(line, x, 0);
        break;
    case 32:
        line[x] = 0;
        break;
    default:
        return ERROR_INT("depth must be in {1,2,4,8,16,32} bpp", __func__, 1);
    }

    return 0;
}


/*!
 * \brief   pixFlipPixel()
 *
 * \param[in]    pix   any depth, warning if colormapped
 * \param[in]    x,y   pixel coords
 * \return  0 if OK; 1 or 2 on error
 *
 * <pre>
 * Notes:
 *      (1) If the point is outside the image, this returns an error (2),
 *          with 0 in %pval.  To avoid spamming output, it fails silently.
 * </pre>
 */
l_ok
pixFlipPixel(PIX     *pix,
             l_int32  x,
             l_int32  y)
{
l_int32    w, h, d, wpl;
l_uint32   val;
l_uint32  *line, *data;

    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    if (pixGetColormap(pix))
        L_WARNING("cmapped: setting to 0 may not be intended\n", __func__);
    pixGetDimensions(pix, &w, &h, &d);
    if (x < 0 || x >= w || y < 0 || y >= h)
        return 2;

    data = pixGetData(pix);
    wpl = pixGetWpl(pix);
    line = data + y * wpl;
    switch (d)
    {
    case 1:
        val = GET_DATA_BIT(line, x);
        if (val)
            CLEAR_DATA_BIT(line, x);
        else
            SET_DATA_BIT(line, x);
        break;
    case 2:
        val = GET_DATA_DIBIT(line, x);
        val ^= 0x3;
        SET_DATA_DIBIT(line, x, val);
        break;
    case 4:
        val = GET_DATA_QBIT(line, x);
        val ^= 0xf;
        SET_DATA_QBIT(line, x, val);
        break;
    case 8:
        val = GET_DATA_BYTE(line, x);
        val ^= 0xff;
        SET_DATA_BYTE(line, x, val);
        break;
    case 16:
        val = GET_DATA_TWO_BYTES(line, x);
        val ^= 0xffff;
        SET_DATA_TWO_BYTES(line, x, val);
        break;
    case 32:
        val = line[x] ^ 0xffffffff;
        line[x] = val;
        break;
    default:
        return ERROR_INT("depth must be in {1,2,4,8,16,32} bpp", __func__, 1);
    }

    return 0;
}


/*!
 * \brief   setPixelLow()
 *
 * \param[in]    line    ptr to beginning of line,
 * \param[in]    x       pixel location in line
 * \param[in]    depth   bpp
 * \param[in]    val     to be inserted
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) Caution: input variables are not checked!
 * </pre>
 */
void
setPixelLow(l_uint32  *line,
            l_int32    x,
            l_int32    depth,
            l_uint32   val)
{
    switch (depth)
    {
    case 1:
        if (val)
            SET_DATA_BIT(line, x);
        else
            CLEAR_DATA_BIT(line, x);
        break;
    case 2:
        SET_DATA_DIBIT(line, x, val);
        break;
    case 4:
        SET_DATA_QBIT(line, x, val);
        break;
    case 8:
        SET_DATA_BYTE(line, x, val);
        break;
    case 16:
        SET_DATA_TWO_BYTES(line, x, val);
        break;
    case 32:
        line[x] = val;
        break;
    default:
        lept_stderr("illegal depth in setPixelLow()\n");
    }
}


/*-------------------------------------------------------------*
 *                     Find black or white value               *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixGetBlackOrWhiteVal()
 *
 * \param[in]    pixs    all depths; cmap ok
 * \param[in]    op      L_GET_BLACK_VAL, L_GET_WHITE_VAL
 * \param[out]   pval    pixel value
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Side effect.  For a colormapped image, if the requested
 *          color is not present and there is room to add it in the cmap,
 *          it is added and the new index is returned.  If there is no room,
 *          the index of the closest color in intensity is returned.
 * </pre>
 */
l_ok
pixGetBlackOrWhiteVal(PIX       *pixs,
                      l_int32    op,
                      l_uint32  *pval)
{
l_int32   d, index;
PIXCMAP  *cmap;

    if (!pval)
        return ERROR_INT("&val not defined", __func__, 1);
    *pval = 0;
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (op != L_GET_BLACK_VAL && op != L_GET_WHITE_VAL)
        return ERROR_INT("invalid op", __func__, 1);

    cmap = pixGetColormap(pixs);
    d = pixGetDepth(pixs);
    if (!cmap) {
        if ((d == 1 && op == L_GET_WHITE_VAL) ||
            (d > 1 && op == L_GET_BLACK_VAL)) {  /* min val */
            *pval = 0;
        } else {  /* max val */
            *pval = (d == 32) ? 0xffffff00 : (1 << d) - 1;
        }
    } else {  /* handle colormap */
        if (op == L_GET_BLACK_VAL)
            pixcmapAddBlackOrWhite(cmap, 0, &index);
        else  /* L_GET_WHITE_VAL */
            pixcmapAddBlackOrWhite(cmap, 1, &index);
        *pval = index;
    }

    return 0;
}


/*-------------------------------------------------------------*
 *     Full image clear/set/set-to-arbitrary-value/invert      *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixClearAll()
 *
 * \param[in]    pix    all depths; use cmapped with caution
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Clears all data to 0.  For 1 bpp, this is white; for grayscale
 *          or color, this is black.
 *      (2) Caution: for colormapped pix, this sets the color to the first
 *          one in the colormap.  Be sure that this is the intended color!
 * </pre>
 */
l_ok
pixClearAll(PIX  *pix)
{
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    memset(pix->data, 0, 4LL * pix->wpl * pix->h);
    return 0;
}


/*!
 * \brief   pixSetAll()
 *
 * \param[in]    pix     all depths; use cmapped with caution
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Sets all data to 1.  For 1 bpp, this is black; for grayscale
 *          or color, this is white.
 *      (2) Caution: for colormapped pix, this sets the pixel value to the
 *          maximum value supported by the colormap: 2^d - 1.  However, this
 *          color may not be defined, because the colormap may not be full.
 * </pre>
 */
l_ok
pixSetAll(PIX  *pix)
{
l_int32   n;
PIXCMAP  *cmap;

    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    if ((cmap = pixGetColormap(pix)) != NULL) {
        n = pixcmapGetCount(cmap);
        if (n < cmap->nalloc)  /* cmap is not full */
            return ERROR_INT("cmap entry does not exist", __func__, 1);
    }

    memset(pix->data, 0xff, 4LL * pix->wpl * pix->h);
    return 0;
}


/*!
 * \brief   pixSetAllGray()
 *
 * \param[in]    pix       all depths, cmap ok
 * \param[in]    grayval   in range 0 ... 255
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) N.B.  For all images, %grayval == 0 represents black and
 *          %grayval == 255 represents white.
 *      (2) For depth < 8, we do our best to approximate the gray level.
 *          For 1 bpp images, any %grayval < 128 is black; >= 128 is white.
 *          For 32 bpp images, each r,g,b component is set to %grayval,
 *          and the alpha component is preserved.
 *      (3) If pix is colormapped, it adds the gray value, replicated in
 *          all components, to the colormap if it's not there and there
 *          is room.  If the colormap is full, it finds the closest color in
 *          L2 distance of components.  This index is written to all pixels.
 * </pre>
 */
l_ok
pixSetAllGray(PIX     *pix,
              l_int32  grayval)
{
l_int32   d, spp, index;
l_uint32  val32;
PIX      *alpha;
PIXCMAP  *cmap;

    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    if (grayval < 0) {
        L_WARNING("grayval < 0; setting to 0\n", __func__);
        grayval = 0;
    } else if (grayval > 255) {
        L_WARNING("grayval > 255; setting to 255\n", __func__);
        grayval = 255;
    }

        /* Handle the colormap case */
    cmap = pixGetColormap(pix);
    if (cmap) {
        pixcmapAddNearestColor(cmap, grayval, grayval, grayval, &index);
        pixSetAllArbitrary(pix, index);
        return 0;
    }

        /* Non-cmapped */
    d = pixGetDepth(pix);
    spp = pixGetSpp(pix);
    if (d == 1) {
        if (grayval < 128)  /* black */
            pixSetAll(pix);
        else
            pixClearAll(pix);  /* white */
    } else if (d < 8) {
        grayval >>= 8 - d;
        pixSetAllArbitrary(pix, grayval);
    } else if (d == 8) {
        pixSetAllArbitrary(pix, grayval);
    } else if (d == 16) {
        grayval |= (grayval << 8);
        pixSetAllArbitrary(pix, grayval);
    } else if (d == 32 && spp == 3) {
        composeRGBPixel(grayval, grayval, grayval, &val32);
        pixSetAllArbitrary(pix, val32);
    } else if (d == 32 && spp == 4) {
        alpha = pixGetRGBComponent(pix, L_ALPHA_CHANNEL);
        composeRGBPixel(grayval, grayval, grayval, &val32);
        pixSetAllArbitrary(pix, val32);
        pixSetRGBComponent(pix, alpha, L_ALPHA_CHANNEL);
        pixDestroy(&alpha);
    } else {
        L_ERROR("invalid depth: %d\n", __func__, d);
        return 1;
    }

    return 0;
}


/*!
 * \brief   pixSetAllArbitrary()
 *
 * \param[in]    pix    all depths; use cmapped with caution
 * \param[in]    val    value to set all pixels
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Caution 1!  For colormapped pix, %val is used as an index
 *          into a colormap.  Be sure that index refers to the intended color.
 *          If the color is not in the colormap, you should first add it
 *          and then call this function.
 *      (2) Caution 2!  For 32 bpp pix, the interpretation of the LSB
 *          of %val depends on whether spp == 3 (RGB) or spp == 4 (RGBA).
 *          For RGB, the LSB is ignored in image transformations.
 *          For RGBA, the LSB is interpreted as the alpha (transparency)
 *          component; full transparency has alpha == 0x0, whereas
 *          full opacity has alpha = 0xff.  An RGBA image with full
 *          opacity behaves like an RGB image.
 *      (3) As an example of (2), suppose you want to initialize a 32 bpp
 *          pix with partial opacity, say 0xee337788.  If the pix is 3 spp,
 *          the 0x88 alpha component will be ignored and may be changed
 *          in subsequent processing.  However, if the pix is 4 spp, the
 *          alpha component will be retained and used. The function
 *          pixCreate(w, h, 32) makes an RGB image by default, and
 *          pixSetSpp(pix, 4) can be used to promote an RGB image to RGBA.
 * </pre>
 */
l_ok
pixSetAllArbitrary(PIX      *pix,
                   l_uint32  val)
{
l_int32    n, i, j, w, h, d, wpl, npix;
l_uint32   maxval, wordval;
l_uint32  *data, *line;
PIXCMAP   *cmap;

    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

        /* If colormapped, make sure that val is less than the size
         * of the cmap array. */
    if ((cmap = pixGetColormap(pix)) != NULL) {
        n = pixcmapGetCount(cmap);
        if (val >= n) {
            L_WARNING("index not in colormap; using last color\n", __func__);
            val = n - 1;
        }
    }

        /* Make sure val isn't too large for the pixel depth.
         * If it is too large, set the pixel color to white.  */
    pixGetDimensions(pix, &w, &h, &d);
    if (d < 32) {
        maxval = (1 << d) - 1;
        if (val > maxval) {
            L_WARNING("val = %d too large for depth; using maxval = %d\n",
                      __func__, val, maxval);
            val = maxval;
        }
    }

        /* Set up word to tile with */
    wordval = 0;
    npix = 32 / d;    /* number of pixels per 32 bit word */
    for (j = 0; j < npix; j++)
        wordval |= (val << (j * d));
    wpl = pixGetWpl(pix);
    data = pixGetData(pix);
    for (i = 0; i < h; i++) {
        line = data + i * wpl;
        for (j = 0; j < wpl; j++) {
            *(line + j) = wordval;
        }
    }
    return 0;
}


/*!
 * \brief   pixSetBlackOrWhite()
 *
 * \param[in]    pixs    all depths; cmap ok
 * \param[in]    op      L_SET_BLACK, L_SET_WHITE
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Function for setting all pixels in an image to either black
 *          or white.
 *      (2) If pixs is colormapped, it adds black or white to the
 *          colormap if it's not there and there is room.  If the colormap
 *          is full, it finds the closest color in intensity.
 *          This index is written to all pixels.
 * </pre>
 */
l_ok
pixSetBlackOrWhite(PIX     *pixs,
                   l_int32  op)
{
l_int32   d, index;
PIXCMAP  *cmap;

    if (!pixs)
        return ERROR_INT("pix not defined", __func__, 1);
    if (op != L_SET_BLACK && op != L_SET_WHITE)
        return ERROR_INT("invalid op", __func__, 1);

    cmap = pixGetColormap(pixs);
    d = pixGetDepth(pixs);
    if (!cmap) {
        if ((d == 1 && op == L_SET_BLACK) || (d > 1 && op == L_SET_WHITE))
            pixSetAll(pixs);
        else
            pixClearAll(pixs);
    } else {  /* handle colormap */
        if (op == L_SET_BLACK)
            pixcmapAddBlackOrWhite(cmap, 0, &index);
        else  /* L_SET_WHITE */
            pixcmapAddBlackOrWhite(cmap, 1, &index);
        pixSetAllArbitrary(pixs, index);
    }

    return 0;
}


/*!
 * \brief   pixSetComponentArbitrary()
 *
 * \param[in]    pix    32 bpp
 * \param[in]    comp   COLOR_RED, COLOR_GREEN, COLOR_BLUE, L_ALPHA_CHANNEL
 * \param[in]    val    value to set this component
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) For example, this can be used to set the alpha component to opaque:
 *              pixSetComponentArbitrary(pix, L_ALPHA_CHANNEL, 255)
 * </pre>
 */
l_ok
pixSetComponentArbitrary(PIX     *pix,
                         l_int32  comp,
                         l_int32  val)
{
l_int32    i, nwords;
l_uint32   mask1, mask2;
l_uint32  *data;

    if (!pix || pixGetDepth(pix) != 32)
        return ERROR_INT("pix not defined or not 32 bpp", __func__, 1);
    if (comp != COLOR_RED && comp != COLOR_GREEN && comp != COLOR_BLUE &&
        comp != L_ALPHA_CHANNEL)
        return ERROR_INT("invalid component", __func__, 1);
    if (val < 0 || val > 255)
        return ERROR_INT("val not in [0 ... 255]", __func__, 1);

    mask1 = ~(255 << (8 * (3 - comp)));
    mask2 = val << (8 * (3 - comp));
    nwords = pixGetHeight(pix) * pixGetWpl(pix);
    data = pixGetData(pix);
    for (i = 0; i < nwords; i++) {
        data[i] &= mask1;  /* clear out the component */
        data[i] |= mask2;  /* insert the new component value */
    }

    return 0;
}


/*-------------------------------------------------------------*
 *     Rectangular region clear/set/set-to-arbitrary-value     *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixClearInRect()
 *
 * \param[in]    pix   all depths; can be cmapped
 * \param[in]    box   in which all pixels will be cleared
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Clears all data in rect to 0.  For 1 bpp, this is white;
 *          for grayscale or color, this is black.
 *      (2) Caution: for colormapped pix, this sets the color to the first
 *          one in the colormap.  Be sure that this is the intended color!
 * </pre>
 */
l_ok
pixClearInRect(PIX  *pix,
               BOX  *box)
{
l_int32  x, y, w, h;

    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    if (!box)
        return ERROR_INT("box not defined", __func__, 1);

    boxGetGeometry(box, &x, &y, &w, &h);
    pixRasterop(pix, x, y, w, h, PIX_CLR, NULL, 0, 0);
    return 0;
}


/*!
 * \brief   pixSetInRect()
 *
 * \param[in]    pix   all depths, can be cmapped
 * \param[in]    box   in which all pixels will be set
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Sets all data in rect to 1.  For 1 bpp, this is black;
 *          for grayscale or color, this is white.
 *      (2) Caution: for colormapped pix, this sets the pixel value to the
 *          maximum value supported by the colormap: 2^d - 1.  However, this
 *          color may not be defined, because the colormap may not be full.
 * </pre>
 */
l_ok
pixSetInRect(PIX  *pix,
             BOX  *box)
{
l_int32   n, x, y, w, h;
PIXCMAP  *cmap;

    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    if (!box)
        return ERROR_INT("box not defined", __func__, 1);
    if ((cmap = pixGetColormap(pix)) != NULL) {
        n = pixcmapGetCount(cmap);
        if (n < cmap->nalloc)  /* cmap is not full */
            return ERROR_INT("cmap entry does not exist", __func__, 1);
    }

    boxGetGeometry(box, &x, &y, &w, &h);
    pixRasterop(pix, x, y, w, h, PIX_SET, NULL, 0, 0);
    return 0;
}


/*!
 * \brief   pixSetInRectArbitrary()
 *
 * \param[in]    pix   all depths; can be cmapped
 * \param[in]    box   in which all pixels will be set to val
 * \param[in]    val   value to set all pixels
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) For colormapped pix, be sure the value is the intended
 *          one in the colormap.
 *      (2) Caution: for colormapped pix, this sets each pixel in the
 *          rect to the color at the index equal to val.  Be sure that
 *          this index exists in the colormap and that it is the intended one!
 * </pre>
 */
l_ok
pixSetInRectArbitrary(PIX      *pix,
                      BOX      *box,
                      l_uint32  val)
{
l_int32    n, x, y, xstart, xend, ystart, yend, bw, bh, w, h, d, wpl;
l_uint32   maxval;
l_uint32  *data, *line;
BOX       *boxc;
PIXCMAP   *cmap;

    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    if (!box)
        return ERROR_INT("box not defined", __func__, 1);
    pixGetDimensions(pix, &w, &h, &d);
    if (d != 1 && d != 2 && d != 4 && d !=8 && d != 16 && d != 32)
        return ERROR_INT("depth must be in {1,2,4,8,16,32} bpp", __func__, 1);
    if ((cmap = pixGetColormap(pix)) != NULL) {
        n = pixcmapGetCount(cmap);
        if (val >= n) {
            L_WARNING("index not in colormap; using last color\n", __func__);
            val = n - 1;
        }
    }

    maxval = (d == 32) ? 0xffffff00 : (1 << d) - 1;
    if (val > maxval) val = maxval;

        /* Handle the simple cases: the min and max values */
    if (val == 0) {
        pixClearInRect(pix, box);
        return 0;
    }
    if (d == 1 ||
        (d == 2 && val == 3) ||
        (d == 4 && val == 0xf) ||
        (d == 8 && val == 0xff) ||
        (d == 16 && val == 0xffff) ||
        (d == 32 && ((val ^ 0xffffff00) >> 8 == 0))) {
        pixSetInRect(pix, box);
        return 0;
    }

        /* Find the overlap of box with the input pix */
    if ((boxc = boxClipToRectangle(box, w, h)) == NULL)
        return ERROR_INT("no overlap of box with image", __func__, 1);
    boxGetGeometry(boxc, &xstart, &ystart, &bw, &bh);
    xend = xstart + bw - 1;
    yend = ystart + bh - 1;
    boxDestroy(&boxc);

    wpl = pixGetWpl(pix);
    data = pixGetData(pix);
    for (y = ystart; y <= yend; y++) {
        line = data + y * wpl;
        for (x = xstart; x <= xend; x++) {
            switch(d)
            {
            case 2:
                SET_DATA_DIBIT(line, x, val);
                break;
            case 4:
                SET_DATA_QBIT(line, x, val);
                break;
            case 8:
                SET_DATA_BYTE(line, x, val);
                break;
            case 16:
                SET_DATA_TWO_BYTES(line, x, val);
                break;
            case 32:
                line[x] = val;
                break;
            default:
                return ERROR_INT("depth not 2|4|8|16|32 bpp", __func__, 1);
            }
        }
    }

    return 0;
}


/*!
 * \brief   pixBlendInRect()
 *
 * \param[in]    pixs   32 bpp rgb
 * \param[in]    box    [optional] in which all pixels will be blended
 * \param[in]    val    blend value; 0xrrggbb00
 * \param[in]    fract  fraction of color to be blended with each pixel in pixs
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is an in-place function.  It blends the input color %val
 *          with the pixels in pixs in the specified rectangle.
 *          If no rectangle is specified, it blends over the entire image.
 * </pre>
 */
l_ok
pixBlendInRect(PIX       *pixs,
               BOX       *box,
               l_uint32   val,
               l_float32  fract)
{
l_int32    i, j, bx, by, bw, bh, w, h, wpls;
l_int32    prval, pgval, pbval, rval, gval, bval;
l_uint32   val32;
l_uint32  *datas, *lines;

    if (!pixs || pixGetDepth(pixs) != 32)
        return ERROR_INT("pixs not defined or not 32 bpp", __func__, 1);

    extractRGBValues(val, &rval, &gval, &bval);
    pixGetDimensions(pixs, &w, &h, NULL);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    if (!box) {
        for (i = 0; i < h; i++) {   /* scan over box */
            lines = datas +  i * wpls;
            for (j = 0; j < w; j++) {
                val32 = *(lines + j);
                extractRGBValues(val32, &prval, &pgval, &pbval);
                prval = (l_int32)((1. - fract) * prval + fract * rval);
                pgval = (l_int32)((1. - fract) * pgval + fract * gval);
                pbval = (l_int32)((1. - fract) * pbval + fract * bval);
                composeRGBPixel(prval, pgval, pbval, &val32);
                *(lines + j) = val32;
            }
        }
        return 0;
    }

    boxGetGeometry(box, &bx, &by, &bw, &bh);
    for (i = 0; i < bh; i++) {   /* scan over box */
        if (by + i < 0 || by + i >= h) continue;
        lines = datas + (by + i) * wpls;
        for (j = 0; j < bw; j++) {
            if (bx + j < 0 || bx + j >= w) continue;
            val32 = *(lines + bx + j);
            extractRGBValues(val32, &prval, &pgval, &pbval);
            prval = (l_int32)((1. - fract) * prval + fract * rval);
            pgval = (l_int32)((1. - fract) * pgval + fract * gval);
            pbval = (l_int32)((1. - fract) * pbval + fract * bval);
            composeRGBPixel(prval, pgval, pbval, &val32);
            *(lines + bx + j) = val32;
        }
    }
    return 0;
}


/*-------------------------------------------------------------*
 *                         Set pad bits                        *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixSetPadBits()
 *
 * \param[in]    pix   1, 2, 4, 8, 16, 32 bpp
 * \param[in]    val   0 or 1
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The pad bits are the bits that expand each scanline to a
 *          multiple of 32 bits.  They are usually not used in
 *          image processing operations.  When boundary conditions
 *          are important, as in seedfill, they must be set properly.
 *      (2) This sets the value of the pad bits (if any) in the last
 *          32-bit word in each scanline.
 *      (3) For 32 bpp pix, there are no pad bits, so this is a no-op.
 *      (4) For 24 bpp pix (which are not generally supported in leptonica),
 *          this operation would affect image components because the pixels
 *          are not aligned with 32-bit word boundaries.
 *      (5) When writing formatted output, such as tiff, png or jpeg,
 *          the pad bits have no effect on the raster image that is
 *          generated by reading back from the file.  However, in some
 *          cases, the compressed file itself will depend on the pad
 *          bits.  This is seen, for example, in Windows with 2 and 4 bpp
 *          tiff-compressed images that have pad bits on each scanline.
 *          It is sometimes convenient to use a golden file with a
 *          byte-by-byte check to verify invariance.  Consequently,
 *          and because setting the pad bits is cheap, the pad bits are
 *          set to 0 before writing these compressed files.
 * </pre>
 */
l_ok
pixSetPadBits(PIX     *pix,
              l_int32  val)
{
l_int32    i, w, h, d, wpl, endbits, fullwords;
l_uint32   mask;
l_uint32  *data, *pword;

    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    pixGetDimensions(pix, &w, &h, &d);
    if (d == 32)  /* no padding exists for 32 bpp */
        return 0;
    if (d == 24) {  /* pixels not aligned with 32-bit words */
        L_INFO("pix is 24 bpp\n", __func__);
        return 1;
    }

    data = pixGetData(pix);
    wpl = pixGetWpl(pix);
    endbits = 32 - (((l_int64)w * d) % 32);
    if (endbits == 32)  /* no partial word */
        return 0;
    fullwords = (1LL * w * d) / 32;
    mask = rmask32[endbits];
    if (val == 0)
        mask = ~mask;

    for (i = 0; i < h; i++) {
        pword = data + i * wpl + fullwords;
        if (val == 0) /* clear */
            *pword = *pword & mask;
        else  /* set */
            *pword = *pword | mask;
    }

    return 0;
}


/*!
 * \brief   pixSetPadBitsBand()
 *
 * \param[in]    pix   1, 2, 4, 8, 16, 32 bpp
 * \param[in]    by    starting y value of band
 * \param[in]    bh    height of band
 * \param[in]    val   0 or 1
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The pad bits are the bits that expand each scanline to a
 *          multiple of 32 bits.  They are usually not used in
 *          image processing operations.  When boundary conditions
 *          are important, as in seedfill, they must be set properly.
 *      (2) This sets the value of the pad bits (if any) in the last
 *          32-bit word in each scanline, within the specified
 *          band of raster lines.
 *      (3) For 32 bpp pix, there are no pad bits, so this is a no-op.
 *          For 24 bpp pix, this function would change image components.
 * </pre>
 */
l_ok
pixSetPadBitsBand(PIX     *pix,
                  l_int32  by,
                  l_int32  bh,
                  l_int32  val)
{
l_int32    i, w, h, d, wpl, endbits, fullwords;
l_uint32   mask;
l_uint32  *data, *pword;

    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    pixGetDimensions(pix, &w, &h, &d);
    if (d == 32)  /* no padding exists for 32 bpp */
        return 0;
    if (d == 24) {  /* pixels not aligned with 32-bit words */
        L_INFO("pix is 24 bpp\n", __func__);
        return 1;
    }

    if (by < 0)
        by = 0;
    if (by >= h)
        return ERROR_INT("start y not in image", __func__, 1);
    if (by + bh > h)
        bh = h - by;

    data = pixGetData(pix);
    wpl = pixGetWpl(pix);
    endbits = 32 - (((l_int64)w * d) % 32);
    if (endbits == 32)  /* no partial word */
        return 0;
    fullwords = (l_int64)w * d / 32;

    mask = rmask32[endbits];
    if (val == 0)
        mask = ~mask;

    for (i = by; i < by + bh; i++) {
        pword = data + i * wpl + fullwords;
        if (val == 0) /* clear */
            *pword = *pword & mask;
        else  /* set */
            *pword = *pword | mask;
    }

    return 0;
}


/*-------------------------------------------------------------*
 *                       Set border pixels                     *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixSetOrClearBorder()
 *
 * \param[in]    pixs                     all depths
 * \param[in]    left, right, top, bot    border region amount to set or clear: these distances are from outside
 * \param[in]    op                       operation PIX_SET or PIX_CLR
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The border region is defined to be the region in the
 *          image within a specific distance of each edge.  Here, we
 *          allow the pixels within a specified distance of each
 *          edge to be set independently.  This either sets or
 *          clears all pixels in the border region.
 *      (2) For binary images, use PIX_SET for black and PIX_CLR for white.
 *      (3) For grayscale or color images, use PIX_SET for white
 *          and PIX_CLR for black.
 * </pre>
 */
l_ok
pixSetOrClearBorder(PIX     *pixs,
                    l_int32  left,
                    l_int32  right,
                    l_int32  top,
                    l_int32  bot,
                    l_int32  op)
{
l_int32  w, h;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (op != PIX_SET && op != PIX_CLR)
        return ERROR_INT("op must be PIX_SET or PIX_CLR", __func__, 1);

    pixGetDimensions(pixs, &w, &h, NULL);
    pixRasterop(pixs, 0, 0, left, h, op, NULL, 0, 0);
    pixRasterop(pixs, w - right, 0, right, h, op, NULL, 0, 0);
    pixRasterop(pixs, 0, 0, w, top, op, NULL, 0, 0);
    pixRasterop(pixs, 0, h - bot, w, bot, op, NULL, 0, 0);

    return 0;
}


/*!
 * \brief   pixSetBorderVal()
 *
 * \param[in]    pixs                   8, 16 or 32 bpp
 * \param[in]    left, right, top, bot  border region amount to set: these distances are from outside
 * \param[in]    val                    value to set at each border pixel
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The border region is defined to be the region in the
 *          image within a specific distance of each edge.  Here, we
 *          allow the pixels within a specified distance of each
 *          edge to be set independently.  This sets the pixels
 *          in the border region to the given input value.
 *      (2) For efficiency, use pixSetOrClearBorder() if
 *          you're setting the border to either black or white.
 *      (3) If d != 32, the input value should be masked off
 *          to the appropriate number of least significant bits.
 *      (4) The code is easily generalized for 2 or 4 bpp.
 * </pre>
 */
l_ok
pixSetBorderVal(PIX      *pixs,
                l_int32   left,
                l_int32   right,
                l_int32   top,
                l_int32   bot,
                l_uint32  val)
{
l_int32    w, h, d, wpls, i, j, bstart, rstart;
l_uint32  *datas, *lines;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 8 && d != 16 && d != 32)
        return ERROR_INT("depth must be 8, 16 or 32 bpp", __func__, 1);

    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    if (d == 8) {
        val &= 0xff;
        for (i = 0; i < top; i++) {
            lines = datas + i * wpls;
            for (j = 0; j < w; j++)
                SET_DATA_BYTE(lines, j, val);
        }
        rstart = w - right;
        bstart = h - bot;
        for (i = top; i < bstart; i++) {
            lines = datas + i * wpls;
            for (j = 0; j < left; j++)
                SET_DATA_BYTE(lines, j, val);
            for (j = rstart; j < w; j++)
                SET_DATA_BYTE(lines, j, val);
        }
        for (i = bstart; i < h; i++) {
            lines = datas + i * wpls;
            for (j = 0; j < w; j++)
                SET_DATA_BYTE(lines, j, val);
        }
    } else if (d == 16) {
        val &= 0xffff;
        for (i = 0; i < top; i++) {
            lines = datas + i * wpls;
            for (j = 0; j < w; j++)
                SET_DATA_TWO_BYTES(lines, j, val);
        }
        rstart = w - right;
        bstart = h - bot;
        for (i = top; i < bstart; i++) {
            lines = datas + i * wpls;
            for (j = 0; j < left; j++)
                SET_DATA_TWO_BYTES(lines, j, val);
            for (j = rstart; j < w; j++)
                SET_DATA_TWO_BYTES(lines, j, val);
        }
        for (i = bstart; i < h; i++) {
            lines = datas + i * wpls;
            for (j = 0; j < w; j++)
                SET_DATA_TWO_BYTES(lines, j, val);
        }
    } else {   /* d == 32 */
        for (i = 0; i < top; i++) {
            lines = datas + i * wpls;
            for (j = 0; j < w; j++)
                *(lines + j) = val;
        }
        rstart = w - right;
        bstart = h - bot;
        for (i = top; i < bstart; i++) {
            lines = datas + i * wpls;
            for (j = 0; j < left; j++)
                *(lines + j) = val;
            for (j = rstart; j < w; j++)
                *(lines + j) = val;
        }
        for (i = bstart; i < h; i++) {
            lines = datas + i * wpls;
            for (j = 0; j < w; j++)
                *(lines + j) = val;
        }
    }

    return 0;
}


/*!
 * \brief   pixSetBorderRingVal()
 *
 * \param[in]    pixs    any depth; cmap OK
 * \param[in]    dist    distance from outside; must be > 0; first ring is 1
 * \param[in]    val     value to set at each border pixel
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The rings are single-pixel-wide rectangular sets of
 *          pixels at a given distance from the edge of the pix.
 *          This sets all pixels in a given ring to a value.
 * </pre>
 */
l_ok
pixSetBorderRingVal(PIX      *pixs,
                    l_int32   dist,
                    l_uint32  val)
{
l_int32  w, h, d, i, j, xend, yend;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (dist < 1)
        return ERROR_INT("dist must be > 0", __func__, 1);
    pixGetDimensions(pixs, &w, &h, &d);
    if (w < 2 * dist + 1 || h < 2 * dist + 1)
        return ERROR_INT("ring doesn't exist", __func__, 1);
    if (d < 32 && (val >= (1 << d)))
        return ERROR_INT("invalid pixel value", __func__, 1);

    xend = w - dist;
    yend = h - dist;
    for (j = dist - 1; j <= xend; j++)
        pixSetPixel(pixs, j, dist - 1, val);
    for (j = dist - 1; j <= xend; j++)
        pixSetPixel(pixs, j, yend, val);
    for (i = dist - 1; i <= yend; i++)
        pixSetPixel(pixs, dist - 1, i, val);
    for (i = dist - 1; i <= yend; i++)
        pixSetPixel(pixs, xend, i, val);

    return 0;
}


/*!
 * \brief   pixSetMirroredBorder()
 *
 * \param[in]    pixs                   all depths; colormap ok
 * \param[in]    left, right, top, bot  number of pixels to set
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This applies what is effectively mirror boundary conditions
 *          to a border region in the image.  It is in-place.
 *      (2) This is useful for setting pixels near the border to a
 *          value representative of the near pixels to the interior.
 *      (3) The general pixRasterop() is used for an in-place operation here
 *          because there is no overlap between the src and dest rectangles.
 * </pre>
 */
l_ok
pixSetMirroredBorder(PIX     *pixs,
                     l_int32  left,
                     l_int32  right,
                     l_int32  top,
                     l_int32  bot)
{
l_int32  i, j, w, h;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);

    pixGetDimensions(pixs, &w, &h, NULL);
    for (j = 0; j < left; j++)
        pixRasterop(pixs, left - 1 - j, top, 1, h - top - bot, PIX_SRC,
                    pixs, left + j, top);
    for (j = 0; j < right; j++)
        pixRasterop(pixs, w - right + j, top, 1, h - top - bot, PIX_SRC,
                    pixs, w - right - 1 - j, top);
    for (i = 0; i < top; i++)
        pixRasterop(pixs, 0, top - 1 - i, w, 1, PIX_SRC,
                    pixs, 0, top + i);
    for (i = 0; i < bot; i++)
        pixRasterop(pixs, 0, h - bot + i, w, 1, PIX_SRC,
                    pixs, 0, h - bot - 1 - i);

    return 0;
}


/*!
 * \brief   pixCopyBorder()
 *
 * \param[in]    pixd                   all depths; colormap ok; can be NULL
 * \param[in]    pixs                   same depth and size as pixd
 * \param[in]    left, right, top, bot  number of pixels to copy
 * \return  pixd, or NULL on error if pixd is not defined
 *
 * <pre>
 * Notes:
 *      (1) pixd can be null, but otherwise it must be the same size
 *          and depth as pixs.  Always returns pixd.
 *      (2) This is useful in situations where by setting a few border
 *          pixels we can avoid having to copy all pixels in pixs into
 *          pixd as an initialization step for some operation.
 *          Nevertheless, for safety, if making a new pixd, all the
 *          non-border pixels are initialized to 0.
 * </pre>
 */
PIX *
pixCopyBorder(PIX     *pixd,
              PIX     *pixs,
              l_int32  left,
              l_int32  right,
              l_int32  top,
              l_int32  bot)
{
l_int32  w, h;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, pixd);

    if (pixd) {
        if (pixd == pixs) {
            L_WARNING("same: nothing to do\n", __func__);
            return pixd;
        } else if (!pixSizesEqual(pixs, pixd)) {
            return (PIX *)ERROR_PTR("pixs and pixd sizes differ",
                                    __func__, pixd);
        }
    } else {
        if ((pixd = pixCreateTemplate(pixs)) == NULL)
            return (PIX *)ERROR_PTR("pixd not made", __func__, pixd);
    }

    pixGetDimensions(pixs, &w, &h, NULL);
    pixRasterop(pixd, 0, 0, left, h, PIX_SRC, pixs, 0, 0);
    pixRasterop(pixd, w - right, 0, right, h, PIX_SRC, pixs, w - right, 0);
    pixRasterop(pixd, 0, 0, w, top, PIX_SRC, pixs, 0, 0);
    pixRasterop(pixd, 0, h - bot, w, bot, PIX_SRC, pixs, 0, h - bot);
    return pixd;
}



/*-------------------------------------------------------------*
 *                     Add and remove border                   *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixAddBorder()
 *
 * \param[in]    pixs   all depths; colormap ok
 * \param[in]    npix   number of pixels to be added to each side
 * \param[in]    val    value of added border pixels
 * \return  pixd with the added exterior pixels, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) See pixGetBlackOrWhiteVal() for values of black and white pixels.
 * </pre>
 */
PIX *
pixAddBorder(PIX      *pixs,
             l_int32   npix,
             l_uint32  val)
{
    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (npix == 0)
        return pixClone(pixs);
    return pixAddBorderGeneral(pixs, npix, npix, npix, npix, val);
}


/*!
 * \brief   pixAddBlackOrWhiteBorder()
 *
 * \param[in]    pixs all depths; colormap ok
 * \param[in]    left, right, top, bot  number of pixels added
 * \param[in]    op L_GET_BLACK_VAL, L_GET_WHITE_VAL
 * \return  pixd with the added exterior pixels, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) See pixGetBlackOrWhiteVal() for possible side effect (adding
 *          a color to a colormap).
 *      (2) The only complication is that pixs may have a colormap.
 *          There are two ways to add the black or white border:
 *          (a) As done here (simplest, most efficient)
 *          (b) l_int32 ws, hs, d;
 *              pixGetDimensions(pixs, &ws, &hs, &d);
 *              Pix *pixd = pixCreate(ws + left + right, hs + top + bot, d);
 *              PixColormap *cmap = pixGetColormap(pixs);
 *              if (cmap != NULL)
 *                  pixSetColormap(pixd, pixcmapCopy(cmap));
 *              pixSetBlackOrWhite(pixd, L_SET_WHITE);  // uses cmap
 *              pixRasterop(pixd, left, top, ws, hs, PIX_SET, pixs, 0, 0);
 * </pre>
 */
PIX *
pixAddBlackOrWhiteBorder(PIX     *pixs,
                         l_int32  left,
                         l_int32  right,
                         l_int32  top,
                         l_int32  bot,
                         l_int32  op)
{
l_uint32  val;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (op != L_GET_BLACK_VAL && op != L_GET_WHITE_VAL)
        return (PIX *)ERROR_PTR("invalid op", __func__, NULL);

    pixGetBlackOrWhiteVal(pixs, op, &val);
    return pixAddBorderGeneral(pixs, left, right, top, bot, val);
}


/*!
 * \brief   pixAddBorderGeneral()
 *
 * \param[in]    pixs                   all depths; colormap ok
 * \param[in]    left, right, top, bot  number of pixels added
 * \param[in]    val                    value of added border pixels
 * \return  pixd with the added exterior pixels, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) For binary images:
 *             white:  val = 0
 *             black:  val = 1
 *          For grayscale images:
 *             white:  val = 2 ** d - 1
 *             black:  val = 0
 *          For rgb color images:
 *             white:  val = 0xffffff00
 *             black:  val = 0
 *          For colormapped images, set val to the appropriate colormap index.
 *      (2) If the added border is either black or white, you can use
 *             pixAddBlackOrWhiteBorder()
 *          The black and white values for all images can be found with
 *             pixGetBlackOrWhiteVal()
 *          which, if pixs is cmapped, may add an entry to the colormap.
 *          Alternatively, if pixs has a colormap, you can find the index
 *          of the pixel whose intensity is closest to white or black:
 *             white: pixcmapGetRankIntensity(cmap, 1.0, &index);
 *             black: pixcmapGetRankIntensity(cmap, 0.0, &index);
 *          and use that for val.
 * </pre>
 */
PIX *
pixAddBorderGeneral(PIX      *pixs,
                    l_int32   left,
                    l_int32   right,
                    l_int32   top,
                    l_int32   bot,
                    l_uint32  val)
{
l_int32   ws, hs, wd, hd, d, op;
l_uint32  maxval;
PIX      *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (left < 0 || right < 0 || top < 0 || bot < 0)
        return (PIX *)ERROR_PTR("negative border added!", __func__, NULL);

    pixGetDimensions(pixs, &ws, &hs, &d);
    wd = ws + left + right;
    hd = hs + top + bot;
    if ((pixd = pixCreate(wd, hd, d)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyResolution(pixd, pixs);
    pixCopyColormap(pixd, pixs);

        /* Set the new border pixels */
    maxval = (d == 32) ? 0xffffff00 : (1 << d) - 1;
    op = UNDEF;
    if (val == 0)
        op = PIX_CLR;
    else if (val >= maxval)
        op = PIX_SET;
    if (op == UNDEF) {
        pixSetAllArbitrary(pixd, val);
    } else {  /* just set or clear the border pixels */
        pixRasterop(pixd, 0, 0, left, hd, op, NULL, 0, 0);
        pixRasterop(pixd, wd - right, 0, right, hd, op, NULL, 0, 0);
        pixRasterop(pixd, 0, 0, wd, top, op, NULL, 0, 0);
        pixRasterop(pixd, 0, hd - bot, wd, bot, op, NULL, 0, 0);
    }

        /* Copy pixs into the interior */
    pixRasterop(pixd, left, top, ws, hs, PIX_SRC, pixs, 0, 0);
    return pixd;
}


/*!
 * \brief   pixAddMultipleBlackWhiteBorders()
 *
 * \param[in]    pixs       all depths; colormap ok
 * \param[in]    nblack1    width of first black border
 * \param[in]    nwhite1    width of first white border
 * \param[in]    nblack2    width of second black border
 * \param[in]    nwhite2    width of second white border
 * \param[in]    nblack3    width of third black border
 * \param[in]    nwhite3    width of third white border
 * \return  pixd with the added borders, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is a convenience function for adding up to 3 black and
 *          3 white borders, alternating black and white.
 *      (2) Each of the 6 args gives the width of the next border, starting
 *          with a black border.  Any of the args can be 0, skipping
 *          the addition of that border.
 *      (3) Maximum allowed border width is 500 for any border.
 * </pre>
 */
PIX *
pixAddMultipleBlackWhiteBorders(PIX      *pixs,
                                l_int32   nblack1,
                                l_int32   nwhite1,
                                l_int32   nblack2,
                                l_int32   nwhite2,
                                l_int32   nblack3,
                                l_int32   nwhite3)
{
l_int32  i, color;
l_int32  w[6];
PIX     *pix1, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);

    w[0] = nblack1;
    w[1] = nwhite1;
    w[2] = nblack2;
    w[3] = nwhite2;
    w[4] = nblack3;
    w[5] = nwhite3;
    pixd = pixClone(pixs);
    for (i = 0; i < 6; i++) {
        if (w[i] > 500)
            L_WARNING("w = %d > 500; skipping\n", __func__, w[i]);
        if (w[i] > 0 && w[i] <= 500) {
            color = (i % 2 == 0) ? L_GET_BLACK_VAL : L_GET_WHITE_VAL;
            pix1 = pixAddBlackOrWhiteBorder(pixd, w[i], w[i], w[i], w[i],
                                            color);
            pixDestroy(&pixd);
            pixd = pix1;
        }
    }

    return pixd;
}


/*!
 * \brief   pixRemoveBorder()
 *
 * \param[in]    pixs   all depths; colormap ok
 * \param[in]    npix   number to be removed from each of the 4 sides
 * \return  pixd with pixels removed around border, or NULL on error
 */
PIX *
pixRemoveBorder(PIX     *pixs,
                l_int32  npix)
{
    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (npix == 0)
        return pixClone(pixs);
    return pixRemoveBorderGeneral(pixs, npix, npix, npix, npix);
}


/*!
 * \brief   pixRemoveBorderGeneral()
 *
 * \param[in]    pixs                   all depths; colormap ok
 * \param[in]    left, right, top, bot  number of pixels removed
 * \return  pixd with pixels removed around border, or NULL on error
 */
PIX *
pixRemoveBorderGeneral(PIX     *pixs,
                       l_int32  left,
                       l_int32  right,
                       l_int32  top,
                       l_int32  bot)
{
l_int32  ws, hs, wd, hd, d;
PIX     *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (left < 0 || right < 0 || top < 0 || bot < 0)
        return (PIX *)ERROR_PTR("negative border removed!", __func__, NULL);

    pixGetDimensions(pixs, &ws, &hs, &d);
    wd = ws - left - right;
    hd = hs - top - bot;
    if (wd <= 0)
        return (PIX *)ERROR_PTR("width must be > 0", __func__, NULL);
    if (hd <= 0)
        return (PIX *)ERROR_PTR("height must be > 0", __func__, NULL);
    if ((pixd = pixCreate(wd, hd, d)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyResolution(pixd, pixs);
    pixCopySpp(pixd, pixs);
    pixCopyColormap(pixd, pixs);

    pixRasterop(pixd, 0, 0, wd, hd, PIX_SRC, pixs, left, top);
    if (pixGetDepth(pixs) == 32 && pixGetSpp(pixs) == 4)
        pixShiftAndTransferAlpha(pixd, pixs, -left, -top);
    return pixd;
}


/*!
 * \brief   pixRemoveBorderToSize()
 *
 * \param[in]    pixs   all depths; colormap ok
 * \param[in]    wd     target width; use 0 if only removing from height
 * \param[in]    hd     target height; use 0 if only removing from width
 * \return  pixd with pixels removed around border, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Removes pixels as evenly as possible from the sides of the
 *          image, leaving the central part.
 *      (2) Returns clone if no pixels requested removed, or the target
 *          sizes are larger than the image.
 * </pre>
 */
PIX *
pixRemoveBorderToSize(PIX     *pixs,
                      l_int32  wd,
                      l_int32  hd)
{
l_int32  w, h, top, bot, left, right, delta;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);

    pixGetDimensions(pixs, &w, &h, NULL);
    if ((wd <= 0 || wd >= w) && (hd <= 0 || hd >= h))
        return pixClone(pixs);

    left = right = (w - wd) / 2;
    delta = w - 2 * left - wd;
    right += delta;
    top = bot = (h - hd) / 2;
    delta = h - hd - 2 * top;
    bot += delta;
    if (wd <= 0 || wd > w)
        left = right = 0;
    else if (hd <= 0 || hd > h)
        top = bot = 0;

    return pixRemoveBorderGeneral(pixs, left, right, top, bot);
}


/*!
 * \brief   pixAddMirroredBorder()
 *
 * \param[in]    pixs                   all depths; colormap ok
 * \param[in]    left, right, top, bot  number of pixels added
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This applies what is effectively mirror boundary conditions.
 *          For the added border pixels in pixd, the pixels in pixs
 *          near the border are mirror-copied into the border region.
 *      (2) This is useful for avoiding special operations near
 *          boundaries when doing image processing operations
 *          such as rank filters and convolution.  In use, one first
 *          adds mirrored pixels to each side of the image.  The number
 *          of pixels added on each side is half the filter dimension.
 *          Then the image processing operations proceed over a
 *          region equal to the size of the original image, and
 *          write directly into a dest pix of the same size as pixs.
 *      (3) The general pixRasterop() is used for an in-place operation here
 *          because there is no overlap between the src and dest rectangles.
 * </pre>
 */
PIX  *
pixAddMirroredBorder(PIX      *pixs,
                      l_int32  left,
                      l_int32  right,
                      l_int32  top,
                      l_int32  bot)
{
l_int32  i, j, w, h;
PIX     *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, NULL);
    if (left > w || right > w || top > h || bot > h)
        return (PIX *)ERROR_PTR("border too large", __func__, NULL);

        /* Set pixels on left, right, top and bottom, in that order */
    pixd = pixAddBorderGeneral(pixs, left, right, top, bot, 0);
    for (j = 0; j < left; j++)
        pixRasterop(pixd, left - 1 - j, top, 1, h, PIX_SRC,
                    pixd, left + j, top);
    for (j = 0; j < right; j++)
        pixRasterop(pixd, left + w + j, top, 1, h, PIX_SRC,
                    pixd, left + w - 1 - j, top);
    for (i = 0; i < top; i++)
        pixRasterop(pixd, 0, top - 1 - i, left + w + right, 1, PIX_SRC,
                    pixd, 0, top + i);
    for (i = 0; i < bot; i++)
        pixRasterop(pixd, 0, top + h + i, left + w + right, 1, PIX_SRC,
                    pixd, 0, top + h - 1 - i);

    return pixd;
}


/*!
 * \brief   pixAddRepeatedBorder()
 *
 * \param[in]    pixs                   all depths; colormap ok
 * \param[in]    left, right, top, bot  number of pixels added
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This applies a repeated border, as if the central part of
 *          the image is tiled over the plane.  So, for example, the
 *          pixels in the left border come from the right side of the image.
 *      (2) The general pixRasterop() is used for an in-place operation here
 *          because there is no overlap between the src and dest rectangles.
 * </pre>
 */
PIX  *
pixAddRepeatedBorder(PIX      *pixs,
                     l_int32  left,
                     l_int32  right,
                     l_int32  top,
                     l_int32  bot)
{
l_int32  w, h;
PIX     *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, NULL);
    if (left > w || right > w || top > h || bot > h)
        return (PIX *)ERROR_PTR("border too large", __func__, NULL);

    pixd = pixAddBorderGeneral(pixs, left, right, top, bot, 0);

        /* Set pixels on left, right, top and bottom, in that order */
    pixRasterop(pixd, 0, top, left, h, PIX_SRC, pixd, w, top);
    pixRasterop(pixd, left + w, top, right, h, PIX_SRC, pixd, left, top);
    pixRasterop(pixd, 0, 0, left + w + right, top, PIX_SRC, pixd, 0, h);
    pixRasterop(pixd, 0, top + h, left + w + right, bot, PIX_SRC, pixd, 0, top);

    return pixd;
}


/*!
 * \brief   pixAddMixedBorder()
 *
 * \param[in]    pixs                   all depths; colormap ok
 * \param[in]    left, right, top, bot  number of pixels added
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This applies mirrored boundary conditions (b.c.) horizontally
 *          and repeated b.c. vertically.
 *      (2) It is specifically used for avoiding special operations
 *          near boundaries when convolving a hue-saturation histogram
 *          with a given window size.  The repeated b.c. are used
 *          vertically for hue, and the mirrored b.c. are used
 *          horizontally for saturation.  The number of pixels added
 *          on each side is approximately (but not quite) half the
 *          filter dimension.  The image processing operations can
 *          then proceed over a region equal to the size of the original
 *          image, and write directly into a dest pix of the same
 *          size as pixs.
 *      (3) The general pixRasterop() can be used for an in-place
 *          operation here because there is no overlap between the
 *          src and dest rectangles.
 * </pre>
 */
PIX  *
pixAddMixedBorder(PIX      *pixs,
                  l_int32  left,
                  l_int32  right,
                  l_int32  top,
                  l_int32  bot)
{
l_int32  j, w, h;
PIX     *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, NULL);
    if (left > w || right > w || top > h || bot > h)
        return (PIX *)ERROR_PTR("border too large", __func__, NULL);

        /* Set mirrored pixels on left and right;
         * then set repeated pixels on top and bottom. */
    pixd = pixAddBorderGeneral(pixs, left, right, top, bot, 0);
    for (j = 0; j < left; j++)
        pixRasterop(pixd, left - 1 - j, top, 1, h, PIX_SRC,
                    pixd, left + j, top);
    for (j = 0; j < right; j++)
        pixRasterop(pixd, left + w + j, top, 1, h, PIX_SRC,
                    pixd, left + w - 1 - j, top);
    pixRasterop(pixd, 0, 0, left + w + right, top, PIX_SRC, pixd, 0, h);
    pixRasterop(pixd, 0, top + h, left + w + right, bot, PIX_SRC, pixd, 0, top);

    return pixd;
}


/*!
 * \brief   pixAddContinuedBorder()
 *
 * \param[in]    pixs                   all depths; colormap ok
 * \param[in]    left, right, top, bot  pixels on each side to be added
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This adds pixels on each side whose values are equal to
 *          the value on the closest boundary pixel.
 * </pre>
 */
PIX *
pixAddContinuedBorder(PIX     *pixs,
                      l_int32  left,
                      l_int32  right,
                      l_int32  top,
                      l_int32  bot)
{
l_int32  i, j, w, h;
PIX     *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);

    pixd = pixAddBorderGeneral(pixs, left, right, top, bot, 0);
    pixGetDimensions(pixs, &w, &h, NULL);
    for (j = 0; j < left; j++)
        pixRasterop(pixd, j, top, 1, h, PIX_SRC, pixd, left, top);
    for (j = 0; j < right; j++)
        pixRasterop(pixd, left + w + j, top, 1, h,
                    PIX_SRC, pixd, left + w - 1, top);
    for (i = 0; i < top; i++)
        pixRasterop(pixd, 0, i, left + w + right, 1, PIX_SRC, pixd, 0, top);
    for (i = 0; i < bot; i++)
        pixRasterop(pixd, 0, top + h + i, left + w + right, 1,
                    PIX_SRC, pixd, 0, top + h - 1);

    return pixd;
}


/*-------------------------------------------------------------------*
 *                     Helper functions using alpha                  *
 *-------------------------------------------------------------------*/
/*!
 * \brief   pixShiftAndTransferAlpha()
 *
 * \param[in]    pixd            32 bpp
 * \param[in]    pixs            32 bpp
 * \param[in]    shiftx, shifty
 * \return  0 if OK; 1 on error
 */
l_ok
pixShiftAndTransferAlpha(PIX       *pixd,
                         PIX       *pixs,
                         l_float32  shiftx,
                         l_float32  shifty)
{
l_int32  w, h;
PIX     *pix1, *pix2;

    if (!pixs || !pixd)
        return ERROR_INT("pixs and pixd not both defined", __func__, 1);
    if (pixGetDepth(pixs) != 32 || pixGetSpp(pixs) != 4)
        return ERROR_INT("pixs not 32 bpp and 4 spp", __func__, 1);
    if (pixGetDepth(pixd) != 32)
        return ERROR_INT("pixd not 32 bpp", __func__, 1);

    if (shiftx == 0 && shifty == 0) {
        pixCopyRGBComponent(pixd, pixs, L_ALPHA_CHANNEL);
        return 0;
    }

    pix1 = pixGetRGBComponent(pixs, L_ALPHA_CHANNEL);
    pixGetDimensions(pixd, &w, &h, NULL);
    pix2 = pixCreate(w, h, 8);
    pixRasterop(pix2, 0, 0, w, h, PIX_SRC, pix1, -shiftx, -shifty);
    pixSetRGBComponent(pixd, pix2, L_ALPHA_CHANNEL);
    pixDestroy(&pix1);
    pixDestroy(&pix2);
    return 0;
}


/*!
 * \brief   pixDisplayLayersRGBA()
 *
 * \param[in]    pixs   cmap or 32 bpp rgba
 * \param[in]    val    32 bit unsigned color to use as background
 * \param[in]    maxw   max output image width; 0 for no scaling
 * \return  pixd showing various image views, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Use %val == 0xffffff00 for white background.
 *      (2) Three views are given:
 *           ~ the image with a fully opaque alpha
 *           ~ the alpha layer
 *           ~ the image as it would appear with a white background.
 * </pre>
 */
PIX *
pixDisplayLayersRGBA(PIX      *pixs,
                     l_uint32  val,
                     l_int32   maxw)
{
l_int32    w, width;
l_float32  scalefact;
PIX       *pix1, *pix2, *pixd;
PIXA      *pixa;
PIXCMAP   *cmap;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    cmap = pixGetColormap(pixs);
    if (!cmap && !(pixGetDepth(pixs) == 32 && pixGetSpp(pixs) == 4))
        return (PIX *)ERROR_PTR("pixs not cmap and not 32 bpp rgba",
                                __func__, NULL);
    if ((w = pixGetWidth(pixs)) == 0)
        return (PIX *)ERROR_PTR("pixs width 0 !!", __func__, NULL);

    if (cmap)
        pix1 = pixRemoveColormap(pixs, REMOVE_CMAP_WITH_ALPHA);
    else
        pix1 = pixCopy(NULL, pixs);

        /* Scale if necessary so the output width is not larger than maxw */
    scalefact = (maxw == 0) ? 1.0f : L_MIN(1.0f, (l_float32)(maxw) / w);
    width = (l_int32)(scalefact * w);

    pixa = pixaCreate(3);
    pixSetSpp(pix1, 3);
    pixaAddPix(pixa, pix1, L_INSERT);  /* show the rgb values */
    pix1 = pixGetRGBComponent(pixs, L_ALPHA_CHANNEL);
    pix2 = pixConvertTo32(pix1);
    pixaAddPix(pixa, pix2, L_INSERT);  /* show the alpha channel */
    pixDestroy(&pix1);
    pix1 = pixAlphaBlendUniform(pixs, (val & 0xffffff00));
    pixaAddPix(pixa, pix1, L_INSERT);  /* with %val color bg showing */
    pixd = pixaDisplayTiledInRows(pixa, 32, width, scalefact, 0, 25, 2);
    pixaDestroy(&pixa);
    return pixd;
}


/*-------------------------------------------------------------*
 *                Color sample setting and extraction          *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixCreateRGBImage()
 *
 * \param[in]   pixr   8 bpp red pix
 * \param[in]   pixg   8 bpp green pix
 * \param[in]   pixb   8 bpp blue pix
 * \return  32 bpp pix, interleaved with 4 samples/pixel,
 *              or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) the 4th byte, sometimes called the "alpha channel",
 *          and which is often used for blending between different
 *          images, is left with 0 value (fully opaque).
 *      (2) see Note (4) in pix.h for details on storage of
 *          8-bit samples within each 32-bit word.
 *      (3) This implementation, setting the r, g and b components
 *          sequentially, is much faster than setting them in parallel
 *          by constructing an RGB dest pixel and writing it to dest.
 *          The reason is there are many more cache misses when reading
 *          from 3 input images simultaneously.
 * </pre>
 */
PIX *
pixCreateRGBImage(PIX  *pixr,
                  PIX  *pixg,
                  PIX  *pixb)
{
l_int32  wr, wg, wb, hr, hg, hb, dr, dg, db;
PIX     *pixd;

    if (!pixr)
        return (PIX *)ERROR_PTR("pixr not defined", __func__, NULL);
    if (!pixg)
        return (PIX *)ERROR_PTR("pixg not defined", __func__, NULL);
    if (!pixb)
        return (PIX *)ERROR_PTR("pixb not defined", __func__, NULL);
    pixGetDimensions(pixr, &wr, &hr, &dr);
    pixGetDimensions(pixg, &wg, &hg, &dg);
    pixGetDimensions(pixb, &wb, &hb, &db);
    if (dr != 8 || dg != 8 || db != 8)
        return (PIX *)ERROR_PTR("input pix not all 8 bpp", __func__, NULL);
    if (wr != wg || wr != wb)
        return (PIX *)ERROR_PTR("widths not the same", __func__, NULL);
    if (hr != hg || hr != hb)
        return (PIX *)ERROR_PTR("heights not the same", __func__, NULL);

    if ((pixd = pixCreate(wr, hr, 32)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyResolution(pixd, pixr);
    pixSetRGBComponent(pixd, pixr, COLOR_RED);
    pixSetRGBComponent(pixd, pixg, COLOR_GREEN);
    pixSetRGBComponent(pixd, pixb, COLOR_BLUE);

    return pixd;
}


/*!
 * \brief   pixGetRGBComponent()
 *
 * \param[in]   pixs   32 bpp, or colormapped
 * \param[in]   comp   one of {COLOR_RED, COLOR_GREEN, COLOR_BLUE,
 *                     L_ALPHA_CHANNEL}
 * \return  pixd the selected 8 bpp component image of the
 *                    input 32 bpp image or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Three calls to this function generate the r, g and b 8 bpp
 *          component images.  This is much faster than generating the
 *          three images in parallel, by extracting a src pixel and setting
 *          the pixels of each component image from it.  The reason is
 *          there are many more cache misses when writing to three
 *          output images simultaneously.
 * </pre>
 */
PIX *
pixGetRGBComponent(PIX     *pixs,
                   l_int32  comp)
{
l_int32    i, j, w, h, wpls, wpld, val;
l_uint32  *lines, *lined;
l_uint32  *datas, *datad;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetColormap(pixs))
        return pixGetRGBComponentCmap(pixs, comp);
    if (pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs not 32 bpp", __func__, NULL);
    if (comp != COLOR_RED && comp != COLOR_GREEN &&
        comp != COLOR_BLUE && comp != L_ALPHA_CHANNEL)
        return (PIX *)ERROR_PTR("invalid comp", __func__, NULL);

    pixGetDimensions(pixs, &w, &h, NULL);
    if ((pixd = pixCreate(w, h, 8)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyResolution(pixd, pixs);
    wpls = pixGetWpl(pixs);
    wpld = pixGetWpl(pixd);
    datas = pixGetData(pixs);
    datad = pixGetData(pixd);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            val = GET_DATA_BYTE(lines + j, comp);
            SET_DATA_BYTE(lined, j, val);
        }
    }

    return pixd;
}


/*!
 * \brief   pixSetRGBComponent()
 *
 * \param[in]   pixd   32 bpp
 * \param[in]   pixs   8 bpp
 * \param[in]   comp   one of the set: {COLOR_RED, COLOR_GREEN,
 *                                      COLOR_BLUE, L_ALPHA_CHANNEL}
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This places the 8 bpp pixel in pixs into the
 *          specified component (properly interleaved) in pixd,
 *      (2) The two images are registered to the UL corner; the sizes
 *          need not be the same, but a warning is issued if they differ.
 * </pre>
 */
l_ok
pixSetRGBComponent(PIX     *pixd,
                   PIX     *pixs,
                   l_int32  comp)
{
l_uint8    srcbyte;
l_int32    i, j, w, h, ws, hs, wd, hd;
l_int32    wpls, wpld;
l_uint32  *lines, *lined;
l_uint32  *datas, *datad;

    if (!pixd)
        return ERROR_INT("pixd not defined", __func__, 1);
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (pixGetDepth(pixd) != 32)
        return ERROR_INT("pixd not 32 bpp", __func__, 1);
    if (pixGetDepth(pixs) != 8)
        return ERROR_INT("pixs not 8 bpp", __func__, 1);
    if (comp != COLOR_RED && comp != COLOR_GREEN &&
        comp != COLOR_BLUE && comp != L_ALPHA_CHANNEL)
        return ERROR_INT("invalid comp", __func__, 1);
    pixGetDimensions(pixs, &ws, &hs, NULL);
    pixGetDimensions(pixd, &wd, &hd, NULL);
    if (ws != wd || hs != hd)
        L_WARNING("images sizes not equal\n", __func__);
    w = L_MIN(ws, wd);
    h = L_MIN(hs, hd);
    if (comp == L_ALPHA_CHANNEL)
        pixSetSpp(pixd, 4);
    datas = pixGetData(pixs);
    datad = pixGetData(pixd);
    wpls = pixGetWpl(pixs);
    wpld = pixGetWpl(pixd);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            srcbyte = GET_DATA_BYTE(lines, j);
            SET_DATA_BYTE(lined + j, comp, srcbyte);
        }
    }

    return 0;
}


/*!
 * \brief   pixGetRGBComponentCmap()
 *
 * \param[in]   pixs   colormapped
 * \param[in]   comp   one of the set: {COLOR_RED, COLOR_GREEN, COLOR_BLUE}
 * \return  pixd  the selected 8 bpp component image of the
 *                     input cmapped image, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) In leptonica, we do not support alpha in colormaps.
 * </pre>
 */
PIX *
pixGetRGBComponentCmap(PIX     *pixs,
                       l_int32  comp)
{
l_int32     i, j, w, h, val, index, valid;
l_int32     wplc, wpld;
l_uint32   *linec, *lined;
l_uint32   *datac, *datad;
PIX        *pixc, *pixd;
PIXCMAP    *cmap;
RGBA_QUAD  *cta;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if ((cmap = pixGetColormap(pixs)) == NULL)
        return (PIX *)ERROR_PTR("pixs not cmapped", __func__, NULL);
    if (comp == L_ALPHA_CHANNEL)
        return (PIX *)ERROR_PTR("alpha in cmaps not supported", __func__, NULL);
    if (comp != COLOR_RED && comp != COLOR_GREEN && comp != COLOR_BLUE)
        return (PIX *)ERROR_PTR("invalid comp", __func__, NULL);

        /* If not 8 bpp, make a cmapped 8 bpp pix */
    if (pixGetDepth(pixs) == 8)
        pixc = pixClone(pixs);
    else
        pixc = pixConvertTo8(pixs, TRUE);
    pixcmapIsValid(cmap, pixc, &valid);
    if (!valid) {
        pixDestroy(&pixc);
        return (PIX *)ERROR_PTR("invalid colormap", __func__, NULL);
    }

    pixGetDimensions(pixs, &w, &h, NULL);
    if ((pixd = pixCreate(w, h, 8)) == NULL) {
        pixDestroy(&pixc);
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    }
    pixCopyResolution(pixd, pixs);
    wplc = pixGetWpl(pixc);
    wpld = pixGetWpl(pixd);
    datac = pixGetData(pixc);
    datad = pixGetData(pixd);
    cta = (RGBA_QUAD *)cmap->array;

    for (i = 0; i < h; i++) {
        linec = datac + i * wplc;
        lined = datad + i * wpld;
        if (comp == COLOR_RED) {
            for (j = 0; j < w; j++) {
                index = GET_DATA_BYTE(linec, j);
                val = cta[index].red;
                SET_DATA_BYTE(lined, j, val);
            }
        } else if (comp == COLOR_GREEN) {
            for (j = 0; j < w; j++) {
                index = GET_DATA_BYTE(linec, j);
                val = cta[index].green;
                SET_DATA_BYTE(lined, j, val);
            }
        } else if (comp == COLOR_BLUE) {
            for (j = 0; j < w; j++) {
                index = GET_DATA_BYTE(linec, j);
                val = cta[index].blue;
                SET_DATA_BYTE(lined, j, val);
            }
        }
    }

    pixDestroy(&pixc);
    return pixd;
}


/*!
 * \brief   pixCopyRGBComponent()
 *
 * \param[in]   pixd   32 bpp
 * \param[in]   pixs   32 bpp
 * \param[in]   comp   one of the set: {COLOR_RED, COLOR_GREEN,
 *                                      COLOR_BLUE, L_ALPHA_CHANNEL}
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The two images are registered to the UL corner.  The sizes
 *          are usually the same, and a warning is issued if they differ.
 * </pre>
 */
l_ok
pixCopyRGBComponent(PIX     *pixd,
                    PIX     *pixs,
                    l_int32  comp)
{
l_int32    i, j, w, h, ws, hs, wd, hd, val;
l_int32    wpls, wpld;
l_uint32  *lines, *lined;
l_uint32  *datas, *datad;

    if (!pixd && pixGetDepth(pixd) != 32)
        return ERROR_INT("pixd not defined or not 32 bpp", __func__, 1);
    if (!pixs && pixGetDepth(pixs) != 32)
        return ERROR_INT("pixs not defined or not 32 bpp", __func__, 1);
    if (comp != COLOR_RED && comp != COLOR_GREEN &&
        comp != COLOR_BLUE && comp != L_ALPHA_CHANNEL)
        return ERROR_INT("invalid component", __func__, 1);
    pixGetDimensions(pixs, &ws, &hs, NULL);
    pixGetDimensions(pixd, &wd, &hd, NULL);
    if (ws != wd || hs != hd)
        L_WARNING("images sizes not equal\n", __func__);
    w = L_MIN(ws, wd);
    h = L_MIN(hs, hd);
    if (comp == L_ALPHA_CHANNEL)
        pixSetSpp(pixd, 4);
    wpls = pixGetWpl(pixs);
    wpld = pixGetWpl(pixd);
    datas = pixGetData(pixs);
    datad = pixGetData(pixd);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            val = GET_DATA_BYTE(lines + j, comp);
            SET_DATA_BYTE(lined + j, comp, val);
        }
    }
    return 0;
}


/*!
 * \brief   composeRGBPixel()
 *
 * \param[in]    rval, gval, bval
 * \param[out]   ppixel             32-bit pixel
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) All channels are 8 bits: the input values must be between
 *          0 and 255.  For speed, this is not enforced by masking
 *          with 0xff before shifting.
 *      (2) A slower implementation uses macros:
 *            SET_DATA_BYTE(ppixel, COLOR_RED, rval);
 *            SET_DATA_BYTE(ppixel, COLOR_GREEN, gval);
 *            SET_DATA_BYTE(ppixel, COLOR_BLUE, bval);
 * </pre>
 */
l_ok
composeRGBPixel(l_int32    rval,
                l_int32    gval,
                l_int32    bval,
                l_uint32  *ppixel)
{
    if (!ppixel)
        return ERROR_INT("&pixel not defined", __func__, 1);

    *ppixel = ((l_uint32)rval << L_RED_SHIFT) |
              ((l_uint32)gval << L_GREEN_SHIFT) |
              ((l_uint32)bval << L_BLUE_SHIFT);
    return 0;
}


/*!
 * \brief   composeRGBAPixel()
 *
 * \param[in]    rval, gval, bval, aval
 * \param[out]   ppixel                  32-bit pixel
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) All channels are 8 bits: the input values must be between
 *          0 and 255.  For speed, this is not enforced by masking
 *          with 0xff before shifting.
 * </pre>
 */
l_ok
composeRGBAPixel(l_int32    rval,
                 l_int32    gval,
                 l_int32    bval,
                 l_int32    aval,
                 l_uint32  *ppixel)
{
    if (!ppixel)
        return ERROR_INT("&pixel not defined", __func__, 1);

    *ppixel = ((l_uint32)rval << L_RED_SHIFT) |
              ((l_uint32)gval << L_GREEN_SHIFT) |
              ((l_uint32)bval << L_BLUE_SHIFT) |
              aval;
    return 0;
}


/*!
 * \brief   extractRGBValues()
 *
 * \param[in]    pixel   32 bit
 * \param[out]   prval   [optional] red component
 * \param[out]   pgval   [optional] green component
 * \param[out]   pbval   [optional] blue component
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) A slower implementation uses macros:
 *             *prval = GET_DATA_BYTE(&pixel, COLOR_RED);
 *             *pgval = GET_DATA_BYTE(&pixel, COLOR_GREEN);
 *             *pbval = GET_DATA_BYTE(&pixel, COLOR_BLUE);
 * </pre>
 */
void
extractRGBValues(l_uint32  pixel,
                 l_int32  *prval,
                 l_int32  *pgval,
                 l_int32  *pbval)
{
    if (prval) *prval = (pixel >> L_RED_SHIFT) & 0xff;
    if (pgval) *pgval = (pixel >> L_GREEN_SHIFT) & 0xff;
    if (pbval) *pbval = (pixel >> L_BLUE_SHIFT) & 0xff;
}


/*!
 * \brief   extractRGBAValues()
 *
 * \param[in]    pixel   32 bit
 * \param[out]   prval   [optional] red component
 * \param[out]   pgval   [optional] green component
 * \param[out]   pbval   [optional] blue component
 * \param[out]   paval   [optional] alpha component
 * \return  void
 */
void
extractRGBAValues(l_uint32  pixel,
                  l_int32  *prval,
                  l_int32  *pgval,
                  l_int32  *pbval,
                  l_int32  *paval)
{
    if (prval) *prval = (pixel >> L_RED_SHIFT) & 0xff;
    if (pgval) *pgval = (pixel >> L_GREEN_SHIFT) & 0xff;
    if (pbval) *pbval = (pixel >> L_BLUE_SHIFT) & 0xff;
    if (paval) *paval = (pixel >> L_ALPHA_SHIFT) & 0xff;
}


/*!
 * \brief   extractMinMaxComponent()
 *
 * \param[in]   pixel   32 bpp RGB
 * \param[in]   type    L_CHOOSE_MIN or L_CHOOSE_MAX
 * \return  component in range [0 ... 255], or NULL on error
 */
l_int32
extractMinMaxComponent(l_uint32  pixel,
                       l_int32   type)
{
l_int32  rval, gval, bval, val;

    extractRGBValues(pixel, &rval, &gval, &bval);
    if (type == L_CHOOSE_MIN) {
        val = L_MIN(rval, gval);
        val = L_MIN(val, bval);
    } else {  /* type == L_CHOOSE_MAX */
        val = L_MAX(rval, gval);
        val = L_MAX(val, bval);
    }
    return val;
}


/*!
 * \brief   pixGetRGBLine()
 *
 * \param[in]   pixs   32 bpp
 * \param[in]   row
 * \param[in]   bufr   array of red samples; size w bytes
 * \param[in]   bufg   array of green samples; size w bytes
 * \param[in]   bufb   array of blue samples; size w bytes
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This puts rgb components from the input line in pixs
 *          into the given buffers.
 * </pre>
 */
l_ok
pixGetRGBLine(PIX      *pixs,
              l_int32   row,
              l_uint8  *bufr,
              l_uint8  *bufg,
              l_uint8  *bufb)
{
l_uint32  *lines;
l_int32    j, w, h;
l_int32    wpls;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (pixGetDepth(pixs) != 32)
        return ERROR_INT("pixs not 32 bpp", __func__, 1);
    if (!bufr || !bufg || !bufb)
        return ERROR_INT("buffer not defined", __func__, 1);

    pixGetDimensions(pixs, &w, &h, NULL);
    if (row < 0 || row >= h)
        return ERROR_INT("row out of bounds", __func__, 1);
    wpls = pixGetWpl(pixs);
    lines = pixGetData(pixs) + row * wpls;

    for (j = 0; j < w; j++) {
        bufr[j] = GET_DATA_BYTE(lines + j, COLOR_RED);
        bufg[j] = GET_DATA_BYTE(lines + j, COLOR_GREEN);
        bufb[j] = GET_DATA_BYTE(lines + j, COLOR_BLUE);
    }

    return 0;
}


/*-------------------------------------------------------------*
 *                   Raster line pixel setter                  *
 *-------------------------------------------------------------*/
/*!
 * \brief   setLineDataVal()
 *
 * \param[in]    line    ptr to first word in raster line data
 * \param[in]    j       index of pixels into the raster line
 * \param[in]    d       depth of the pixel
 * \param[in]    val     pixel value to be set
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is a convenience function to set a pixel value in a
 *          raster line where the depth of the image can have different
 *          values (1, 2, 4, 8, 16 or 32).
 * </pre>
 */
l_ok
setLineDataVal(l_uint32  *line,
               l_int32    j,
               l_int32    d,
               l_uint32   val)
{
    if (!line)
        return ERROR_INT("line not defined", __func__, 1);
    if (j < 0)
        return ERROR_INT("j must be >= 0", __func__, 1);
    if (d != 1 && d != 2 && d != 4 && d != 8 && d != 16 && d != 32)
        return ERROR_INT("invalid d", __func__, 1);

    if (d == 1)
        SET_DATA_BIT_VAL(line, j, val);
    else if (d == 2)
        SET_DATA_DIBIT(line, j, val);
    else if (d == 4)
        SET_DATA_QBIT(line, j, val);
    else if (d == 8)
        SET_DATA_BYTE(line, j, val);
    else if (d == 16)
        SET_DATA_TWO_BYTES(line, j, val);
    else  /* d == 32 */
        *(line + j) = val;
    return 0;
}


/*-------------------------------------------------------------*
 *                    Pixel endian conversion                  *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixEndianByteSwapNew()
 *
 * \param[in]    pixs
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is used to convert the data in a pix to a
 *          serialized byte buffer in raster order, and, for RGB,
 *          in order RGBA.  This requires flipping bytes within
 *          each 32-bit word for little-endian platforms, because the
 *          words have a MSB-to-the-left rule, whereas byte raster-order
 *          requires the left-most byte in each word to be byte 0.
 *          For big-endians, no swap is necessary, so this returns a clone.
 *      (2) Unlike pixEndianByteSwap(), which swaps the bytes in-place,
 *          this returns a new pix (or a clone).  We provide this
 *          because often when serialization is done, the source
 *          pix needs to be restored to canonical little-endian order,
 *          and this requires a second byte swap.  In such a situation,
 *          it is twice as fast to make a new pix in big-endian order,
 *          use it, and destroy it.
 * </pre>
 */
PIX *
pixEndianByteSwapNew(PIX  *pixs)
{
l_uint32  *datas, *datad;
l_int32    i, j, h, wpl;
l_uint32   word;
PIX       *pixd;

#ifdef L_BIG_ENDIAN

    return pixClone(pixs);

#else   /* L_LITTLE_ENDIAN */

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);

    datas = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    h = pixGetHeight(pixs);
    if ((pixd = pixCreateTemplate(pixs)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    datad = pixGetData(pixd);
    for (i = 0; i < h; i++) {
        for (j = 0; j < wpl; j++, datas++, datad++) {
            word = *datas;
            *datad = (word >> 24) |
                    ((word >> 8) & 0x0000ff00) |
                    ((word << 8) & 0x00ff0000) |
                    (word << 24);
        }
    }

    return pixd;

#endif   /* L_BIG_ENDIAN */

}


/*!
 * \brief   pixEndianByteSwap()
 *
 * \param[in]   pixs
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is used on little-endian platforms to swap
 *          the bytes within a word; bytes 0 and 3 are swapped,
 *          and bytes 1 and 2 are swapped.
 *      (2) This is required for little-endians in situations
 *          where we convert from a serialized byte order that is
 *          in raster order, as one typically has in file formats,
 *          to one with MSB-to-the-left in each 32-bit word, or v.v.
 *          See pix.h for a description of the canonical format
 *          (MSB-to-the left) that is used for both little-endian
 *          and big-endian platforms.   For big-endians, the
 *          MSB-to-the-left word order has the bytes in raster
 *          order when serialized, so no byte flipping is required.
 * </pre>
 */
l_ok
pixEndianByteSwap(PIX  *pixs)
{
l_uint32  *data;
l_int32    i, j, h, wpl;
l_uint32   word;

#ifdef L_BIG_ENDIAN

    return 0;

#else   /* L_LITTLE_ENDIAN */

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);

    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    h = pixGetHeight(pixs);
    for (i = 0; i < h; i++) {
        for (j = 0; j < wpl; j++, data++) {
            word = *data;
            *data = (word >> 24) |
                    ((word >> 8) & 0x0000ff00) |
                    ((word << 8) & 0x00ff0000) |
                    (word << 24);
        }
    }

    return 0;

#endif   /* L_BIG_ENDIAN */

}


/*!
 * \brief   lineEndianByteSwap()
 *
 * \param[in]  datad   dest byte array data, reordered on little-endians
 * \param[in]  datas   a src line of pix data)
 * \param[in]  wpl     number of 32 bit words in the line
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is used on little-endian platforms to swap
 *          the bytes within each word in the line of image data.
 *          Bytes 0 <==> 3 and 1 <==> 2 are swapped in the dest
 *          byte array data8d, relative to the pix data in datas.
 *      (2) The bytes represent 8 bit pixel values.  They are swapped
 *          for little endians so that when the dest array datad
 *          is addressed by bytes, the pixels are chosen sequentially
 *          from left to right in the image.
 * </pre>
 */
l_int32
lineEndianByteSwap(l_uint32  *datad,
                   l_uint32  *datas,
                   l_int32    wpl)
{
l_int32   j;
l_uint32  word;

    if (!datad || !datas)
        return ERROR_INT("datad and datas not both defined", __func__, 1);

#ifdef L_BIG_ENDIAN

    memcpy(datad, datas, 4 * wpl);
    return 0;

#else   /* L_LITTLE_ENDIAN */

    for (j = 0; j < wpl; j++, datas++, datad++) {
        word = *datas;
        *datad = (word >> 24) |
                 ((word >> 8) & 0x0000ff00) |
                 ((word << 8) & 0x00ff0000) |
                 (word << 24);
    }
    return 0;

#endif   /* L_BIG_ENDIAN */

}


/*!
 * \brief   pixEndianTwoByteSwapNew()
 *
 * \param[in]    pixs
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is used on little-endian platforms to swap the
 *          2-byte entities within a 32-bit word.
 *      (2) This is equivalent to a full byte swap, as performed
 *          by pixEndianByteSwap(), followed by byte swaps in
 *          each of the 16-bit entities separately.
 *      (3) Unlike pixEndianTwoByteSwap(), which swaps the shorts in-place,
 *          this returns a new pix (or a clone).  We provide this
 *          to avoid having to swap twice in situations where the input
 *          pix must be restored to canonical little-endian order.
 * </pre>
 */
PIX *
pixEndianTwoByteSwapNew(PIX  *pixs)
{
l_uint32  *datas, *datad;
l_int32    i, j, h, wpl;
l_uint32   word;
PIX       *pixd;

#ifdef L_BIG_ENDIAN

    return pixClone(pixs);

#else   /* L_LITTLE_ENDIAN */

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);

    datas = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    h = pixGetHeight(pixs);
    if ((pixd = pixCreateTemplate(pixs)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    datad = pixGetData(pixd);
    for (i = 0; i < h; i++) {
        for (j = 0; j < wpl; j++, datas++, datad++) {
            word = *datas;
            *datad = (word << 16) | (word >> 16);
        }
    }

    return pixd;

#endif   /* L_BIG_ENDIAN */

}


/*!
 * \brief   pixEndianTwoByteSwap()
 *
 * \param[in]    pixs
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is used on little-endian platforms to swap the
 *          2-byte entities within a 32-bit word.
 *      (2) This is equivalent to a full byte swap, as performed
 *          by pixEndianByteSwap(), followed by byte swaps in
 *          each of the 16-bit entities separately.
 * </pre>
 */
l_ok
pixEndianTwoByteSwap(PIX  *pixs)
{
l_uint32  *data;
l_int32    i, j, h, wpl;
l_uint32   word;

#ifdef L_BIG_ENDIAN

    return 0;

#else   /* L_LITTLE_ENDIAN */

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);

    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    h = pixGetHeight(pixs);
    for (i = 0; i < h; i++) {
        for (j = 0; j < wpl; j++, data++) {
            word = *data;
            *data = (word << 16) | (word >> 16);
        }
    }

    return 0;

#endif   /* L_BIG_ENDIAN */

}


/*-------------------------------------------------------------*
 *             Extract raster data as binary string            *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixGetRasterData()
 *
 * \param[in]    pixs     1, 8, 32 bpp
 * \param[out]   pdata    raster data in memory
 * \param[out]   pnbytes  number of bytes in data string
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This returns the raster data as a byte string, padded to the
 *          byte.  For 1 bpp, the first pixel is the MSbit in the first byte.
 *          For rgb, the bytes are in (rgb) order.  This is the format
 *          required for flate encoding of pixels in a PostScript file.
 * </pre>
 */
l_ok
pixGetRasterData(PIX       *pixs,
                 l_uint8  **pdata,
                 size_t    *pnbytes)
{
l_int32    w, h, d, wpl, i, j, rval, gval, bval;
l_int32    databpl;  /* bytes for each raster line in returned data */
l_uint8   *line, *data;  /* packed data in returned array */
l_uint32  *rline, *rdata;  /* data in pix raster */

    if (pdata) *pdata = NULL;
    if (pnbytes) *pnbytes = 0;
    if (!pdata || !pnbytes)
        return ERROR_INT("&data and &nbytes not both defined", __func__, 1);
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 1 && d != 2 && d != 4 && d != 8 && d != 16 && d != 32)
        return ERROR_INT("depth not in {1,2,4,8,16,32}", __func__, 1);

    pixSetPadBits(pixs, 0);
    rdata = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    if (d == 1)
        databpl = (w + 7) / 8;
    else if (d == 2)
        databpl = (w + 3) / 4;
    else if (d == 4)
        databpl = (w + 1) / 2;
    else if (d == 8 || d == 16)
        databpl = w * (d / 8);
    else  /* d == 32 bpp rgb */
        databpl = 3 * w;
    if ((data = (l_uint8 *)LEPT_CALLOC((size_t)databpl * h, sizeof(l_uint8)))
            == NULL)
        return ERROR_INT("data not allocated", __func__, 1);
    *pdata = data;
    *pnbytes = (size_t)databpl * h;

    for (i = 0; i < h; i++) {
         rline = rdata + i * wpl;
         line = data + i * databpl;
         if (d <= 8) {
             for (j = 0; j < databpl; j++)
                  line[j] = GET_DATA_BYTE(rline, j);
         } else if (d == 16) {
             for (j = 0; j < w; j++)
                  line[2 * j] = GET_DATA_TWO_BYTES(rline, j);
         } else {  /* d == 32 bpp rgb */
             for (j = 0; j < w; j++) {
                  extractRGBValues(rline[j], &rval, &gval, &bval);
                  *(line + 3 * j) = rval;
                  *(line + 3 * j + 1) = gval;
                  *(line + 3 * j + 2) = bval;
             }
         }
    }

    return 0;
}


/*-------------------------------------------------------------*
 *                Infer resolution from image size             *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixInferResolution()
 *
 * \param[in]    pix
 * \param[in]    longside    assumed max dimension, in inches
 * \param[out]   pres        resolution (ppi)
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This finds the resolution, assuming that the longest side
 *          of the image is %longside.  On error, returns 300 ppi.
 *      (2) This is useful for computing resolution for generating pdfs,
 *          when the images are scanned from pages of known size.
 *          There, %longside is typically about 11.0.
 * </pre>
 */
l_ok
pixInferResolution(PIX       *pix,
                   l_float32  longside,
                   l_int32   *pres)
{
l_int32  w, h, maxdim, res;

    if (!pres)
        return ERROR_INT("&res not defined", __func__, 1);
    *pres = 300;
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    if (longside <= 0.0)
        return ERROR_INT("longside not > 0", __func__, 1);

    pixGetDimensions(pix, &w, &h, NULL);
    maxdim = L_MAX(w, h);
    res = (l_int32)(maxdim / longside + 0.5);
    res = L_MAX(res, 1);  /* don't let it be 0 */
    if (res < 10)
        L_WARNING("low inferred resolution: %d ppi\n", __func__, res);
    if (res > 10000)
        L_WARNING("high inferred resolution: %d ppi\n", __func__, res);
    *pres = res;
    return 0;
}


/*-------------------------------------------------------------*
 *                 Test alpha component opaqueness             *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixAlphaIsOpaque()
 *
 * \param[in]    pix       32 bpp, spp == 4
 * \param[out]   popaque   1 if spp == 4 and all alpha component
 *                         values are 255 (opaque); 0 otherwise
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      1) On error, opaque is returned as 0 (FALSE).
 * </pre>
 */
l_ok
pixAlphaIsOpaque(PIX      *pix,
                 l_int32  *popaque)
{
l_int32    w, h, wpl, i, j, alpha;
l_uint32  *data, *line;

    if (!popaque)
        return ERROR_INT("&opaque not defined", __func__, 1);
    *popaque = FALSE;
    if (!pix)
        return ERROR_INT("&pix not defined", __func__, 1);
    if (pixGetDepth(pix) != 32)
        return ERROR_INT("&pix not 32 bpp", __func__, 1);
    if (pixGetSpp(pix) != 4)
        return ERROR_INT("&pix not 4 spp", __func__, 1);

    data = pixGetData(pix);
    wpl = pixGetWpl(pix);
    pixGetDimensions(pix, &w, &h, NULL);
    for (i = 0; i < h; i++) {
        line = data + i * wpl;
        for (j = 0; j < w; j++) {
            alpha = GET_DATA_BYTE(line + j, L_ALPHA_CHANNEL);
            if (alpha ^ 0xff)  /* not opaque */
                return 0;
        }
    }

    *popaque = TRUE;
    return 0;
}


/*-------------------------------------------------------------*
 *             Setup helpers for 8 bpp byte processing         *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixSetupByteProcessing()
 *
 * \param[in]    pix   8 bpp, no colormap
 * \param[out]   pw    [optional] width
 * \param[out]   ph    [optional] height
 * \return  line ptr array, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is a simple helper for processing 8 bpp images with
 *          direct byte access.  It can swap byte order within each word.
 *      (2) After processing, you must call pixCleanupByteProcessing(),
 *          which frees the lineptr array and restores byte order.
 *      (3) Usage:
 *              l_uint8 **lineptrs = pixSetupByteProcessing(pix, &w, &h);
 *              for (i = 0; i < h; i++) {
 *                  l_uint8 *line = lineptrs[i];
 *                  for (j = 0; j < w; j++) {
 *                      val = line[j];
 *                      ...
 *                  }
 *              }
 *              pixCleanupByteProcessing(pix, lineptrs);
 * </pre>
 */
l_uint8 **
pixSetupByteProcessing(PIX      *pix,
                       l_int32  *pw,
                       l_int32  *ph)
{
l_int32  w, h;

    if (pw) *pw = 0;
    if (ph) *ph = 0;
    if (!pix || pixGetDepth(pix) != 8)
        return (l_uint8 **)ERROR_PTR("pix not defined or not 8 bpp",
                                     __func__, NULL);
    pixGetDimensions(pix, &w, &h, NULL);
    if (pw) *pw = w;
    if (ph) *ph = h;
    if (pixGetColormap(pix))
        return (l_uint8 **)ERROR_PTR("pix has colormap", __func__, NULL);

    pixEndianByteSwap(pix);
    return (l_uint8 **)pixGetLinePtrs(pix, NULL);
}


/*!
 * \brief   pixCleanupByteProcessing()
 *
 * \param[in]   pix        8 bpp, no colormap
 * \param[in]   lineptrs   ptrs to the beginning of each raster line of data
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This must be called after processing that was initiated
 *          by pixSetupByteProcessing() has finished.
 * </pre>
 */
l_ok
pixCleanupByteProcessing(PIX      *pix,
                         l_uint8 **lineptrs)
{
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    if (!lineptrs)
        return ERROR_INT("lineptrs not defined", __func__, 1);

    pixEndianByteSwap(pix);
    LEPT_FREE(lineptrs);
    return 0;
}


/*------------------------------------------------------------------------*
 *      Setting parameters for antialias masking with alpha transforms    *
 *------------------------------------------------------------------------*/
/*!
 * \brief   l_setAlphaMaskBorder()
 *
 * \param[in]    val1, val2     in [0.0 ... 1.0]
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) This sets the opacity values used to generate the two outer
 *          boundary rings in the alpha mask associated with geometric
 *          transforms such as pixRotateWithAlpha().
 *      (2) The default values are val1 = 0.0 (completely transparent
 *          in the outermost ring) and val2 = 0.5 (half transparent
 *          in the second ring).  When the image is blended, this
 *          completely removes the outer ring (shrinking the image by
 *          2 in each direction), and alpha-blends with 0.5 the second ring.
 *          Using val1 = 0.25 and val2 = 0.75 gives a slightly more
 *          blurred border, with no perceptual difference at screen resolution.
 *      (3) The actual mask values are found by multiplying these
 *          normalized opacity values by 255.
 * </pre>
 */
void
l_setAlphaMaskBorder(l_float32  val1,
                     l_float32  val2)
{
    val1 = L_MAX(0.0f, L_MIN(1.0f, val1));
    val2 = L_MAX(0.0f, L_MIN(1.0f, val2));
    AlphaMaskBorderVals[0] = val1;
    AlphaMaskBorderVals[1] = val2;
}
