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
 * \file psio2.c
 * <pre>
 *
 *    |=============================================================|
 *    |                         Important note                      |
 *    |=============================================================|
 *    | Some of these functions require I/O libraries such as       |
 *    | libtiff, libjpeg, and libz.  If you do not have these       |
 *    | libraries, some calls will fail.                            |
 *    |                                                             |
 *    | You can manually deactivate all PostScript writing by       |
 *    | setting this in environ.h:                                  |
 *    | \code                                                       |
 *    |     #define  USE_PSIO     0                                 |
 *    | \endcode                                                    |
 *    | in environ.h.  This will link psio2stub.c                   |
 *    |=============================================================|
 *
 *     These are lower-level functions that implement a PostScript
 *     "device driver" for wrapping images in PostScript.  The images
 *     can be rendered by a PostScript interpreter for viewing,
 *     using evince or gv.  They can also be rasterized for printing,
 *     using gs or an embedded interpreter in a PostScript printer.
 *     And they can be converted to a pdf using gs (ps2pdf).
 *
 *     For uncompressed images
 *          l_int32              pixWritePSEmbed()
 *          l_int32              pixWriteStreamPS()
 *          char                *pixWriteStringPS()
 *          char                *generateUncompressedPS()
 *          static void          getScaledParametersPS()
 *          static l_int32       convertByteToHexAscii()
 *
 *     For jpeg compressed images (use dct compression)
 *          l_int32              convertJpegToPSEmbed()
 *          l_int32              convertJpegToPS()
 *          static l_int32       convertJpegToPSString()
 *          static char         *generateJpegPS()
 *
 *     For g4 fax compressed images (use ccitt g4 compression)
 *          l_int32              convertG4ToPSEmbed()
 *          l_int32              convertG4ToPS()
 *          static l_int32       convertG4ToPSString()
 *          static char         *generateG4PS()
 *
 *     For multipage tiff images
 *          l_int32              convertTiffMultipageToPS()
 *
 *     For flate (gzip) compressed images (e.g., png)
 *          l_int32              convertFlateToPSEmbed()
 *          l_int32              convertFlateToPS()
 *          static l_int32       convertFlateToPSString()
 *          static char         *generateFlatePS()
 *
 *     Write to memory
 *          l_int32              pixWriteMemPS()
 *
 *     Converting resolution
 *          l_int32              getResLetterPage()
 *          static l_int32       getResA4Page()
 *
 *     Setting flag for writing bounding box hint
 *          void                 l_psWriteBoundingBox()
 *
 *  See psio1.c for higher-level functions and their usage.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"

/* --------------------------------------------*/
#if  USE_PSIO   /* defined in environ.h */
 /* --------------------------------------------*/

    /* Set default for writing bounding box hint */
static l_int32  var_PS_WRITE_BOUNDING_BOX = 1;

#define Bufsize 512
static const l_int32  DefaultInputRes = 300;  /* typical scan res, ppi */
static const l_int32  MinRes          = 5;
static const l_int32  MaxRes          = 3000;

    /* For computing resolution that fills page to desired amount */
static const l_int32  LetterWidth  = 612;   /* points */
static const l_int32  LetterHeight = 792;   /* points */
static const l_int32  A4Width      = 595;   /* points */
static const l_int32  A4Height     = 842;   /* points */
static const l_float32  DefaultFillFraction = 0.95f;

#ifndef  NO_CONSOLE_IO
#define  DEBUG_JPEG       0
#define  DEBUG_G4         0
#define  DEBUG_FLATE      0
#endif  /* ~NO_CONSOLE_IO */

/* Note that the bounding box hint at the top of the generated PostScript
 * file is required for the "*Embed" functions.  These generate a
 * PostScript file for an individual image that can be translated and
 * scaled by an application that embeds the image in its output
 * (e.g., in the PS output from a TeX file).
 * However, bounding box hints should not be embedded in any
 * PostScript image that will be composited with other images,
 * where more than one image may be placed in an arbitrary location
 * on a page.  */

    /* Static helper functions */
static void getScaledParametersPS(BOX *box, l_int32 wpix, l_int32 hpix,
                                  l_int32 res, l_float32 scale,
                                  l_float32 *pxpt, l_float32 *pypt,
                                  l_float32 *pwpt, l_float32 *phpt);
static void convertByteToHexAscii(l_uint8 byteval, char *pnib1, char *pnib2);
static l_ok convertJpegToPSString(const char *filein, char **poutstr,
                                  l_int32 *pnbytes, l_int32 x, l_int32 y,
                                  l_int32 res, l_float32 scale,
                                  l_int32 pageno, l_int32 endpage);
static char *generateJpegPS(const char *filein, L_COMP_DATA *cid,
                            l_float32 xpt, l_float32 ypt, l_float32 wpt,
                            l_float32 hpt, l_int32 pageno, l_int32 endpage);
static l_ok convertG4ToPSString(const char *filein, char **poutstr,
                                l_int32 *pnbytes, l_int32 x, l_int32 y,
                                l_int32 res, l_float32 scale, l_int32 pageno,
                                l_int32 maskflag, l_int32 endpage);
static char *generateG4PS(const char *filein, L_COMP_DATA *cid, l_float32 xpt,
                          l_float32 ypt, l_float32 wpt, l_float32 hpt,
                          l_int32 maskflag, l_int32 pageno, l_int32 endpage);
static l_ok convertFlateToPSString(const char *filein, char **poutstr,
                                   l_int32 *pnbytes, l_int32 x, l_int32 y,
                                   l_int32 res, l_float32 scale,
                                   l_int32 pageno, l_int32 endpage);
static char *generateFlatePS(const char *filein, L_COMP_DATA *cid,
                             l_float32 xpt, l_float32 ypt, l_float32 wpt,
                             l_float32 hpt, l_int32 pageno, l_int32 endpage);


/*-------------------------------------------------------------*
 *                  For uncompressed images                    *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixWritePSEmbed()
 *
 * \param[in]    filein    input file, all depths, colormap OK
 * \param[in]    fileout   output ps file
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is a simple wrapper function that generates an
 *          uncompressed PS file, with a bounding box.
 *      (2) The bounding box is required when a program such as TeX
 *          (through epsf) places and rescales the image.
 *      (3) The bounding box is sized for fitting the image to an
 *          8.5 x 11.0 inch page.
 * </pre>
 */
l_ok
pixWritePSEmbed(const char  *filein,
                const char  *fileout)
{
l_int32    w, h, ret;
l_float32  scale;
FILE      *fp;
PIX       *pix;

    if (!filein)
        return ERROR_INT("filein not defined", __func__, 1);
    if (!fileout)
        return ERROR_INT("fileout not defined", __func__, 1);

    if ((pix = pixRead(filein)) == NULL)
        return ERROR_INT("image not read from file", __func__, 1);
    w = pixGetWidth(pix);
    h = pixGetHeight(pix);
    if (w * 11.0 > h * 8.5)
        scale = 8.5f * 300.f / (l_float32)w;
    else
        scale = 11.0f * 300.f / (l_float32)h;

    if ((fp = fopenWriteStream(fileout, "wb")) == NULL)
        return ERROR_INT_1("file not opened for write", fileout, __func__, 1);
    ret = pixWriteStreamPS(fp, pix, NULL, 0, scale);
    fclose(fp);

    pixDestroy(&pix);
    return ret;
}


/*!
 * \brief   pixWriteStreamPS()
 *
 * \param[in]    fp      file stream
 * \param[in]    pix
 * \param[in]    box     [optional]
 * \param[in]    res     can use 0 for default of 300 ppi
 * \param[in]    scale   to prevent scaling, use either 1.0 or 0.0
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This writes image in PS format, optionally scaled,
 *          adjusted for the printer resolution, and with
 *          a bounding box.
 *      (2) For details on use of parameters, see pixWriteStringPS().
 * </pre>
 */
l_ok
pixWriteStreamPS(FILE      *fp,
                 PIX       *pix,
                 BOX       *box,
                 l_int32    res,
                 l_float32  scale)
{
char    *outstr;
l_int32  length;
PIX     *pixc;

    if (!fp)
        return (l_int32)ERROR_INT("stream not open", __func__, 1);
    if (!pix)
        return (l_int32)ERROR_INT("pix not defined", __func__, 1);

    if ((pixc = pixConvertForPSWrap(pix)) == NULL)
        return (l_int32)ERROR_INT("pixc not made", __func__, 1);

    if ((outstr = pixWriteStringPS(pixc, box, res, scale)) == NULL) {
        pixDestroy(&pixc);
        return (l_int32)ERROR_INT("outstr not made", __func__, 1);
    }
    length = strlen(outstr);
    fwrite(outstr, 1, length, fp);
    LEPT_FREE(outstr);
    pixDestroy(&pixc);
    return 0;
}


/*!
 * \brief   pixWriteStringPS()
 *
 * \param[in]    pixs   all depths, colormap OK
 * \param[in]    box    bounding box; can be NULL
 * \param[in]    res    resolution, in printer ppi.  Use 0 for default 300 ppi.
 * \param[in]    scale  scale factor.  If no scaling is desired, use
 *                      either 1.0 or 0.0.   Scaling just resets the resolution
 *                      parameter; the actual scaling is done in the
 *                      interpreter at rendering time.  This is important:
 *                      it allows you to scale the image up without
 *                      increasing the file size.
 * \return  ps string if OK, or NULL on error
 *
 * <pre>
 * a) If %box == NULL, image is placed, optionally scaled,
 *      in a standard b.b. at the center of the page.
 *      This is to be used when another program like
 *      TeX through epsf places the image.
 * b) If %box != NULL, image is placed without a
 *      b.b. at the specified page location and with
 *      optional scaling.  This is to be used when
 *      you want to specify exactly where and optionally
 *      how big you want the image to be.
 *      Note that all coordinates are in PS convention,
 *      with 0,0 at LL corner of the page:
 *          x,y    location of LL corner of image, in mils.
 *          w,h    scaled size, in mils.  Use 0 to
 *                 scale with "scale" and "res" input.
 *
 * %scale: If no scaling is desired, use either 1.0 or 0.0.
 * Scaling just resets the resolution parameter; the actual
 * scaling is done in the interpreter at rendering time.
 * This is important: * it allows you to scale the image up
 * without increasing the file size.
 *
 * Notes:
 *      (1) OK, this seems a bit complicated, because there are various
 *          ways to scale and not to scale.  Here's a summary:
 *      (2) If you don't want any scaling at all:
 *           * if you are using a box:
 *               set w = 0, h = 0, and use scale = 1.0; it will print
 *               each pixel unscaled at printer resolution
 *           * if you are not using a box:
 *               set scale = 1.0; it will print at printer resolution
 *      (3) If you want the image to be a certain size in inches:
 *           * you must use a box and set the box (w,h) in mils
 *      (4) If you want the image to be scaled by a scale factor != 1.0:
 *           * if you are using a box:
 *               set w = 0, h = 0, and use the desired scale factor;
 *               the higher the printer resolution, the smaller the
 *               image will actually appear.
 *           * if you are not using a box:
 *               set the desired scale factor; the higher the printer
 *               resolution, the smaller the image will actually appear.
 *      (5) Another complication is the proliferation of distance units:
 *           * The interface distances are in milli-inches.
 *           * Three different units are used internally:
 *              ~ pixels  (units of 1/res inch)
 *              ~ printer pts (units of 1/72 inch)
 *              ~ inches
 *           * Here is a quiz on volume units from a reviewer:
 *             How many UK milli-cups in a US kilo-teaspoon?
 *               (Hint: 1.0 US cup = 0.75 UK cup + 0.2 US gill;
 *                      1.0 US gill = 24.0 US teaspoons)
 * </pre>
 */
char *
pixWriteStringPS(PIX       *pixs,
                 BOX       *box,
                 l_int32    res,
                 l_float32  scale)
{
char       nib1, nib2;
char      *hexdata, *outstr;
l_uint8    byteval;
l_int32    i, j, k, w, h, d;
l_float32  wpt, hpt, xpt, ypt;
l_int32    wpl, psbpl, hexbytes, boxflag, bps;
l_uint32  *line, *data;
PIX       *pix;

    if (!pixs)
        return (char *)ERROR_PTR("pixs not defined", __func__, NULL);

    if ((pix = pixConvertForPSWrap(pixs)) == NULL)
        return (char *)ERROR_PTR("pix not made", __func__, NULL);
    pixGetDimensions(pix, &w, &h, &d);

        /* Get the factors by which PS scales and translates, in pts */
    if (!box)
        boxflag = 0;  /* no scaling; b.b. at center */
    else
        boxflag = 1;  /* no b.b., specify placement and optional scaling */
    getScaledParametersPS(box, w, h, res, scale, &xpt, &ypt, &wpt, &hpt);

    if (d == 1)
        bps = 1;  /* bits/sample */
    else  /* d == 8 || d == 32 */
        bps = 8;

        /* Convert image data to hex string.  psbpl is the number of
         * bytes in each raster line when it is packed to the byte
         * boundary (not the 32 bit word boundary, as with the pix).
         * When converted to hex, the hex string has 2 bytes for
         * every byte of raster data. */
    wpl = pixGetWpl(pix);
    if (d == 1 || d == 8)
        psbpl = (w * d + 7) / 8;
    else /* d == 32 */
        psbpl = 3 * w;
    data = pixGetData(pix);
    hexbytes = 2 * psbpl * h;  /* size of ps hex array */
    if ((hexdata = (char *)LEPT_CALLOC(hexbytes + 1, sizeof(char))) == NULL)
        return (char *)ERROR_PTR("hexdata not made", __func__, NULL);
    if (d == 1 || d == 8) {
        for (i = 0, k = 0; i < h; i++) {
            line = data + i * wpl;
            for (j = 0; j < psbpl; j++) {
                byteval = GET_DATA_BYTE(line, j);
                convertByteToHexAscii(byteval, &nib1, &nib2);
                hexdata[k++] = nib1;
                hexdata[k++] = nib2;
            }
        }
    } else  {  /* d == 32; hexdata bytes packed RGBRGB..., 2 per sample */
        for (i = 0, k = 0; i < h; i++) {
            line = data + i * wpl;
            for (j = 0; j < w; j++) {
                byteval = GET_DATA_BYTE(line + j, 0);  /* red */
                convertByteToHexAscii(byteval, &nib1, &nib2);
                hexdata[k++] = nib1;
                hexdata[k++] = nib2;
                byteval = GET_DATA_BYTE(line + j, 1);  /* green */
                convertByteToHexAscii(byteval, &nib1, &nib2);
                hexdata[k++] = nib1;
                hexdata[k++] = nib2;
                byteval = GET_DATA_BYTE(line + j, 2);  /* blue */
                convertByteToHexAscii(byteval, &nib1, &nib2);
                hexdata[k++] = nib1;
                hexdata[k++] = nib2;
            }
        }
    }
    hexdata[k] = '\0';

    outstr = generateUncompressedPS(hexdata, w, h, d, psbpl, bps,
                                    xpt, ypt, wpt, hpt, boxflag);
    pixDestroy(&pix);
    if (!outstr)
        return (char *)ERROR_PTR("outstr not made", __func__, NULL);
    return outstr;
}


/*!
 * \brief   generateUncompressedPS()
 *
 * \param[in]    hexdata
 * \param[in]    w, h       raster image size in pixels
 * \param[in]    d          image depth in bpp; rgb is 32
 * \param[in]    psbpl      raster bytes/line, when packed to the byte boundary
 * \param[in]    bps        bits/sample: either 1 or 8
 * \param[in]    xpt, ypt   location of LL corner of image, in pts, relative
 *                          to the PostScript origin (0,0) at the LL corner
 *                          of the page
 * \param[in]    wpt, hpt   rendered image size in pts
 * \param[in]    boxflag    1 to print out bounding box hint; 0 to skip
 * \return  PS string, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Low-level function.
 * </pre>
 */
char *
generateUncompressedPS(char      *hexdata,
                       l_int32    w,
                       l_int32    h,
                       l_int32    d,
                       l_int32    psbpl,
                       l_int32    bps,
                       l_float32  xpt,
                       l_float32  ypt,
                       l_float32  wpt,
                       l_float32  hpt,
                       l_int32    boxflag)
{
char    *outstr;
char     bigbuf[Bufsize];
SARRAY  *sa;

    if (!hexdata)
        return (char *)ERROR_PTR("hexdata not defined", __func__, NULL);

    sa = sarrayCreate(0);
    sarrayAddString(sa, "%!Adobe-PS", L_COPY);
    if (boxflag == 0) {
        snprintf(bigbuf, sizeof(bigbuf),
                 "%%%%BoundingBox: %7.2f %7.2f %7.2f %7.2f",
                 xpt, ypt, xpt + wpt, ypt + hpt);
        sarrayAddString(sa, bigbuf, L_COPY);
    } else {  /* boxflag == 1 */
        sarrayAddString(sa, "gsave", L_COPY);
    }

    if (d == 1)
        sarrayAddString(sa,
              "{1 exch sub} settransfer    %invert binary", L_COPY);

    snprintf(bigbuf, sizeof(bigbuf),
            "/bpl %d string def         %%bpl as a string", psbpl);
    sarrayAddString(sa, bigbuf, L_COPY);
    snprintf(bigbuf, sizeof(bigbuf),
           "%7.2f %7.2f translate         %%set image origin in pts", xpt, ypt);
    sarrayAddString(sa, bigbuf, L_COPY);
    snprintf(bigbuf, sizeof(bigbuf),
            "%7.2f %7.2f scale             %%set image size in pts", wpt, hpt);
    sarrayAddString(sa, bigbuf, L_COPY);
    snprintf(bigbuf, sizeof(bigbuf),
            "%d %d %d                 %%image dimensions in pixels", w, h, bps);
    sarrayAddString(sa, bigbuf, L_COPY);
    snprintf(bigbuf, sizeof(bigbuf),
            "[%d %d %d %d %d %d]     %%mapping matrix: [w 0 0 -h 0 h]",
            w, 0, 0, -h, 0, h);
    sarrayAddString(sa, bigbuf, L_COPY);

    if (boxflag == 0) {
        if (d == 1 || d == 8)
            sarrayAddString(sa,
                "{currentfile bpl readhexstring pop} image", L_COPY);
        else  /* d == 32 */
            sarrayAddString(sa,
                "{currentfile bpl readhexstring pop} false 3 colorimage",
                L_COPY);
    } else {  /* boxflag == 1 */
        if (d == 1 || d == 8)
            sarrayAddString(sa,
                "{currentfile bpl readhexstring pop} bind image", L_COPY);
        else  /* d == 32 */
            sarrayAddString(sa,
                "{currentfile bpl readhexstring pop} bind false 3 colorimage",
                L_COPY);
    }

    sarrayAddString(sa, hexdata, L_INSERT);

    if (boxflag == 0)
        sarrayAddString(sa, "\nshowpage", L_COPY);
    else  /* boxflag == 1 */
        sarrayAddString(sa, "\ngrestore", L_COPY);

    outstr = sarrayToString(sa, 1);
    sarrayDestroy(&sa);
    if (!outstr) L_ERROR("outstr not made\n", __func__);
    return outstr;
}


/*!
 * \brief   getScaledParametersPS()
 *
 * \param[in]    box     [optional] location of image in mils; x,y is LL corner
 * \param[in]    wpix    pix width in pixels
 * \param[in]    hpix    pix height in pixels
 * \param[in]    res     of printer; use 0 for default
 * \param[in]    scale   use 1.0 or 0.0 for no scaling
 * \param[out]   pxpt    location of llx in pts
 * \param[out]   pypt    location of lly in pts
 * \param[out]   pwpt    image width in pts
 * \param[out]   phpt    image height in pts
 * \return  void no arg checking
 *
 * <pre>
 * Notes:
 *      (1) The image is always scaled, depending on res and scale.
 *      (2) If no box, the image is centered on the page.
 *      (3) If there is a box, the image is placed within it.
 * </pre>
 */
static void
getScaledParametersPS(BOX        *box,
                      l_int32     wpix,
                      l_int32     hpix,
                      l_int32     res,
                      l_float32   scale,
                      l_float32  *pxpt,
                      l_float32  *pypt,
                      l_float32  *pwpt,
                      l_float32  *phpt)
{
l_int32    bx, by, bw, bh;
l_float32  winch, hinch, xinch, yinch, fres;

    if (res == 0)
        res = DefaultInputRes;
    fres = (l_float32)res;

        /* Allow the PS interpreter to scale the resolution */
    if (scale == 0.0)
        scale = 1.0;
    if (scale != 1.0) {
        fres = (l_float32)res / scale;
        res = (l_int32)fres;
    }

        /* Limit valid resolution interval */
    if (res < MinRes || res > MaxRes) {
        L_WARNING("res %d out of bounds; using default res; no scaling\n",
                  __func__, res);
        res = DefaultInputRes;
        fres = (l_float32)res;
    }

    if (!box) {  /* center on page */
        winch = (l_float32)wpix / fres;
        hinch = (l_float32)hpix / fres;
        xinch = (8.5f - winch) / 2.f;
        yinch = (11.0f - hinch) / 2.f;
    } else {
        boxGetGeometry(box, &bx, &by, &bw, &bh);
        if (bw == 0)
            winch = (l_float32)wpix / fres;
        else
            winch = (l_float32)bw / 1000.f;
        if (bh == 0)
            hinch = (l_float32)hpix / fres;
        else
            hinch = (l_float32)bh / 1000.f;
        xinch = (l_float32)bx / 1000.f;
        yinch = (l_float32)by / 1000.f;
    }

    if (xinch < 0)
        L_WARNING("left edge < 0.0 inch\n", __func__);
    if (xinch + winch > 8.5)
        L_WARNING("right edge > 8.5 inch\n", __func__);
    if (yinch < 0.0)
        L_WARNING("bottom edge < 0.0 inch\n", __func__);
    if (yinch + hinch > 11.0)
        L_WARNING("top edge > 11.0 inch\n", __func__);

    *pwpt = 72.f * winch;
    *phpt = 72.f * hinch;
    *pxpt = 72.f * xinch;
    *pypt = 72.f * yinch;
    return;
}


/*!
 * \brief   convertByteToHexAscii()
 *
 * \param[in]    byteval        input byte
 * \param[out]   pnib1, pnib2   two hex ascii characters
 * \return  void
 */
static void
convertByteToHexAscii(l_uint8  byteval,
                      char    *pnib1,
                      char    *pnib2)
{
l_uint8  nib;

    nib = byteval >> 4;
    if (nib < 10)
        *pnib1 = '0' + nib;
    else
        *pnib1 = 'a' + (nib - 10);
    nib = byteval & 0xf;
    if (nib < 10)
        *pnib2 = '0' + nib;
    else
        *pnib2 = 'a' + (nib - 10);
    return;
}


/*-------------------------------------------------------------*
 *                  For jpeg compressed images                 *
 *-------------------------------------------------------------*/
/*!
 * \brief   convertJpegToPSEmbed()
 *
 * \param[in]    filein    input jpeg file
 * \param[in]    fileout   output ps file
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This function takes a jpeg file as input and generates a DCT
 *          compressed, ascii85 encoded PS file, with a bounding box.
 *      (2) The bounding box is required when a program such as TeX
 *          (through epsf) places and rescales the image.
 *      (3) The bounding box is sized for fitting the image to an
 *          8.5 x 11.0 inch page.
 * </pre>
 */
l_ok
convertJpegToPSEmbed(const char  *filein,
                     const char  *fileout)
{
char         *outstr;
l_int32       w, h, nbytes, ret;
l_float32     xpt, ypt, wpt, hpt;
L_COMP_DATA  *cid;

    if (!filein)
        return ERROR_INT("filein not defined", __func__, 1);
    if (!fileout)
        return ERROR_INT("fileout not defined", __func__, 1);

        /* Generate the ascii encoded jpeg data */
    if ((cid = l_generateJpegData(filein, 1)) == NULL)
        return ERROR_INT("jpeg data not made", __func__, 1);
    w = cid->w;
    h = cid->h;

        /* Scale for 20 pt boundary and otherwise full filling
         * in one direction on 8.5 x 11 inch device */
    xpt = 20.0;
    ypt = 20.0;
    if (w * 11.0 > h * 8.5) {
        wpt = 572.0;   /* 612 - 2 * 20 */
        hpt = wpt * (l_float32)h / (l_float32)w;
    } else {
        hpt = 752.0;   /* 792 - 2 * 20 */
        wpt = hpt * (l_float32)w / (l_float32)h;
    }

        /* Generate the PS.
         * The bounding box information should be inserted (default). */
    outstr = generateJpegPS(NULL, cid, xpt, ypt, wpt, hpt, 1, 1);
    l_CIDataDestroy(&cid);
    if (!outstr)
        return ERROR_INT("outstr not made", __func__, 1);
    nbytes = strlen(outstr);

    ret = l_binaryWrite(fileout, "w", outstr, nbytes);
    LEPT_FREE(outstr);
    if (ret) L_ERROR("ps string not written to file\n", __func__);
    return ret;
}


/*!
 * \brief   convertJpegToPS()
 *
 * \param[in]    filein     input jpeg file
 * \param[in]    fileout    output ps file
 * \param[in]    operation  "w" for write; "a" for append
 * \param[in]    x, y       location of LL corner of image, in pixels, relative
 *                          to the PostScript origin (0,0) at the LL corner
 *                          of the page
 * \param[in]    res        resolution of the input image, in ppi;
 *                          use 0 for default
 * \param[in]    scale      scaling by printer; use 0.0 or 1.0 for no scaling
 * \param[in]    pageno     page number; must start with 1; you can use 0
 *                          if there is only one page
 * \param[in]    endpage    boolean: use TRUE if this is the last image to be
 *                          added to the page; FALSE otherwise
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is simpler to use than pixWriteStringPS(), and
 *          it outputs in level 2 PS as compressed DCT (overlaid
 *          with ascii85 encoding).
 *      (2) An output file can contain multiple pages, each with
 *          multiple images.  The arguments to convertJpegToPS()
 *          allow you to control placement of jpeg images on multiple
 *          pages within a PostScript file.
 *      (3) For the first image written to a file, use "w", which
 *          opens for write and clears the file.  For all subsequent
 *          images written to that file, use "a".
 *      (4) The (x, y) parameters give the LL corner of the image
 *          relative to the LL corner of the page.  They are in
 *          units of pixels if scale = 1.0.  If you use (e.g.)
 *          scale = 2.0, the image is placed at (2x, 2y) on the page,
 *          and the image dimensions are also doubled.
 *      (5) Display vs printed resolution:
 *           * If your display is 75 ppi and your image was created
 *             at a resolution of 300 ppi, you can get the image
 *             to print at the same size as it appears on your display
 *             by either setting scale = 4.0 or by setting  res = 75.
 *             Both tell the printer to make a 4x enlarged image.
 *           * If your image is generated at 150 ppi and you use scale = 1,
 *             it will be rendered such that 150 pixels correspond
 *             to 72 pts (1 inch on the printer).  This function does
 *             the conversion from pixels (with or without scaling) to
 *             pts, which are the units that the printer uses.
 *           * The printer will choose its own resolution to use
 *             in rendering the image, which will not affect the size
 *             of the rendered image.  That is because the output
 *             PostScript file describes the geometry in terms of pts,
 *             which are defined to be 1/72 inch.  The printer will
 *             only see the size of the image in pts, through the
 *             scale and translate parameters and the affine
 *             transform (the ImageMatrix) of the image.
 *      (6) To render multiple images on the same page, set
 *          endpage = FALSE for each image until you get to the
 *          last, for which you set endpage = TRUE.  This causes the
 *          "showpage" command to be invoked.  Showpage outputs
 *          the entire page and clears the raster buffer for the
 *          next page to be added.  Without a "showpage",
 *          subsequent images from the next page will overlay those
 *          previously put down.
 *      (7) For multiple pages, increment the page number, starting
 *          with page 1.  This allows PostScript (and PDF) to build
 *          a page directory, which viewers use for navigation.
 * </pre>
 */
l_ok
convertJpegToPS(const char  *filein,
                const char  *fileout,
                const char  *operation,
                l_int32      x,
                l_int32      y,
                l_int32      res,
                l_float32    scale,
                l_int32      pageno,
                l_int32      endpage)
{
char    *outstr;
l_int32  nbytes;

    if (!filein)
        return ERROR_INT("filein not defined", __func__, 1);
    if (!fileout)
        return ERROR_INT("fileout not defined", __func__, 1);
    if (strcmp(operation, "w") && strcmp(operation, "a"))
        return ERROR_INT("operation must be \"w\" or \"a\"", __func__, 1);

    if (convertJpegToPSString(filein, &outstr, &nbytes, x, y, res, scale,
                          pageno, endpage))
        return ERROR_INT("ps string not made", __func__, 1);

    if (l_binaryWrite(fileout, operation, outstr, nbytes)) {
        LEPT_FREE(outstr);
        return ERROR_INT("ps string not written to file", __func__, 1);
    }

    LEPT_FREE(outstr);
    return 0;
}


/*!
 * \brief   convertJpegToPSString()
 *
 *      Generates PS string in jpeg format from jpeg file
 *
 * \param[in]    filein     input jpeg file
 * \param[out]   poutstr    PS string
 * \param[out]   pnbytes    number of bytes in PS string
 * \param[in]    x, y       location of LL corner of image, in pixels, relative
 *                          to the PostScript origin (0,0) at the LL corner
 *                           of the page
 * \param[in]    res        resolution of the input image, in ppi;
 *                          use 0 for default
 * \param[in]    scale      scaling by printer; use 0.0 or 1.0 for no scaling
 * \param[in]    pageno     page number; must start with 1; you can use 0
 *                          if there is only one page
 * \param[in]    endpage    boolean: use TRUE if this is the last image to be
 *                          added to the page; FALSE otherwise
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) For usage, see convertJpegToPS()
 * </pre>
 */
static l_ok
convertJpegToPSString(const char  *filein,
                      char       **poutstr,
                      l_int32     *pnbytes,
                      l_int32      x,
                      l_int32      y,
                      l_int32      res,
                      l_float32    scale,
                      l_int32      pageno,
                      l_int32      endpage)
{
char         *outstr;
l_float32     xpt, ypt, wpt, hpt;
L_COMP_DATA  *cid;

    if (!poutstr)
        return ERROR_INT("&outstr not defined", __func__, 1);
    if (!pnbytes)
        return ERROR_INT("&nbytes not defined", __func__, 1);
    *poutstr = NULL;
    *pnbytes = 0;
    if (!filein)
        return ERROR_INT("filein not defined", __func__, 1);

        /* Generate the ascii encoded jpeg data */
    if ((cid = l_generateJpegData(filein, 1)) == NULL)
        return ERROR_INT("jpeg data not made", __func__, 1);

        /* Get scaled location in pts.  Guess the input scan resolution
         * based on the input parameter %res, the resolution data in
         * the pix, and the size of the image. */
    if (scale == 0.0)
        scale = 1.0;
    if (res <= 0) {
        if (cid->res > 0)
            res = cid->res;
        else
            res = DefaultInputRes;
    }

        /* Get scaled location in pts */
    if (scale == 0.0)
        scale = 1.0;
    xpt = scale * x * 72.f / res;
    ypt = scale * y * 72.f / res;
    wpt = scale * cid->w * 72.f / res;
    hpt = scale * cid->h * 72.f / res;

    if (pageno == 0)
        pageno = 1;

#if  DEBUG_JPEG
    lept_stderr("w = %d, h = %d, bps = %d, spp = %d\n",
                cid->w, cid->h, cid->bps, cid->spp);
    lept_stderr("comp bytes = %ld, nbytes85 = %ld, ratio = %5.3f\n",
                (unsigned long)cid->nbytescomp, (unsigned long)cid->nbytes85,
                (l_float32)cid->nbytes85 / (l_float32)cid->nbytescomp);
    lept_stderr("xpt = %7.2f, ypt = %7.2f, wpt = %7.2f, hpt = %7.2f\n",
                xpt, ypt, wpt, hpt);
#endif   /* DEBUG_JPEG */

        /* Generate the PS */
    outstr = generateJpegPS(NULL, cid, xpt, ypt, wpt, hpt, pageno, endpage);
    l_CIDataDestroy(&cid);
    if (!outstr)
        return ERROR_INT("outstr not made", __func__, 1);
    *poutstr = outstr;
    *pnbytes = strlen(outstr);
    return 0;
}


/*!
 * \brief   generateJpegPS()
 *
 * \param[in]    filein     [optional] input jpeg filename; can be null
 * \param[in]    cid        jpeg compressed image data
 * \param[in]    xpt, ypt   location of LL corner of image, in pts, relative
 *                          to the PostScript origin (0,0) at the LL corner
 *                          of the page
 * \param[in]    wpt, hpt   rendered image size in pts
 * \param[in]    pageno     page number; must start with 1; you can use 0
 *                          if there is only one page.
 * \param[in]    endpage    boolean: use TRUE if this is the last image to be
 *                          added to the page; FALSE otherwise
 * \return  PS string, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Low-level function.
 * </pre>
 */
static char *
generateJpegPS(const char   *filein,
               L_COMP_DATA  *cid,
               l_float32     xpt,
               l_float32     ypt,
               l_float32     wpt,
               l_float32     hpt,
               l_int32       pageno,
               l_int32       endpage)
{
l_int32  w, h, bps, spp;
char    *outstr;
char     bigbuf[Bufsize];
SARRAY  *sa;

    if (!cid)
        return (char *)ERROR_PTR("jpeg data not defined", __func__, NULL);
    w = cid->w;
    h = cid->h;
    bps = cid->bps;
    spp = cid->spp;

    sa = sarrayCreate(50);
    sarrayAddString(sa, "%!PS-Adobe-3.0", L_COPY);
    sarrayAddString(sa, "%%Creator: leptonica", L_COPY);
    if (filein)
        snprintf(bigbuf, sizeof(bigbuf), "%%%%Title: %s", filein);
    else
        snprintf(bigbuf, sizeof(bigbuf), "%%%%Title: Jpeg compressed PS");
    sarrayAddString(sa, bigbuf, L_COPY);
    sarrayAddString(sa, "%%DocumentData: Clean7Bit", L_COPY);

    if (var_PS_WRITE_BOUNDING_BOX == 1) {
        snprintf(bigbuf, sizeof(bigbuf),
                 "%%%%BoundingBox: %7.2f %7.2f %7.2f %7.2f",
                 xpt, ypt, xpt + wpt, ypt + hpt);
        sarrayAddString(sa, bigbuf, L_COPY);
    }

    sarrayAddString(sa, "%%LanguageLevel: 2", L_COPY);
    sarrayAddString(sa, "%%EndComments", L_COPY);
    snprintf(bigbuf, sizeof(bigbuf), "%%%%Page: %d %d", pageno, pageno);
    sarrayAddString(sa, bigbuf, L_COPY);

    sarrayAddString(sa, "save", L_COPY);
    sarrayAddString(sa,
                    "/RawData currentfile /ASCII85Decode filter def", L_COPY);
    sarrayAddString(sa, "/Data RawData << >> /DCTDecode filter def", L_COPY);

    snprintf(bigbuf, sizeof(bigbuf),
        "%7.2f %7.2f translate         %%set image origin in pts", xpt, ypt);
    sarrayAddString(sa, bigbuf, L_COPY);

    snprintf(bigbuf, sizeof(bigbuf),
        "%7.2f %7.2f scale             %%set image size in pts", wpt, hpt);
    sarrayAddString(sa, bigbuf, L_COPY);

    if (spp == 1)
        sarrayAddString(sa, "/DeviceGray setcolorspace", L_COPY);
    else if (spp == 3)
        sarrayAddString(sa, "/DeviceRGB setcolorspace", L_COPY);
    else  /*spp == 4 */
        sarrayAddString(sa, "/DeviceCMYK setcolorspace", L_COPY);

    sarrayAddString(sa, "{ << /ImageType 1", L_COPY);
    snprintf(bigbuf, sizeof(bigbuf), "     /Width %d", w);
    sarrayAddString(sa, bigbuf, L_COPY);
    snprintf(bigbuf, sizeof(bigbuf), "     /Height %d", h);
    sarrayAddString(sa, bigbuf, L_COPY);
    snprintf(bigbuf, sizeof(bigbuf),
            "     /ImageMatrix [ %d 0 0 %d 0 %d ]", w, -h, h);
    sarrayAddString(sa, bigbuf, L_COPY);
    sarrayAddString(sa, "     /DataSource Data", L_COPY);
    snprintf(bigbuf, sizeof(bigbuf), "     /BitsPerComponent %d", bps);
    sarrayAddString(sa, bigbuf, L_COPY);

    if (spp == 1)
        sarrayAddString(sa, "     /Decode [0 1]", L_COPY);
    else if (spp == 3)
        sarrayAddString(sa, "     /Decode [0 1 0 1 0 1]", L_COPY);
    else   /* spp == 4 */
        sarrayAddString(sa, "     /Decode [0 1 0 1 0 1 0 1]", L_COPY);

    sarrayAddString(sa, "  >> image", L_COPY);
    sarrayAddString(sa, "  Data closefile", L_COPY);
    sarrayAddString(sa, "  RawData flushfile", L_COPY);
    if (endpage == TRUE)
        sarrayAddString(sa, "  showpage", L_COPY);
    sarrayAddString(sa, "  restore", L_COPY);
    sarrayAddString(sa, "} exec", L_COPY);

        /* Insert the ascii85 jpeg data; this is now owned by sa */
    sarrayAddString(sa, cid->data85, L_INSERT);
    cid->data85 = NULL;  /* it has been transferred and destroyed */

        /* Generate and return the output string */
    outstr = sarrayToString(sa, 1);
    sarrayDestroy(&sa);
    return outstr;
}


/*-------------------------------------------------------------*
 *                  For ccitt g4 compressed images             *
 *-------------------------------------------------------------*/
/*!
 * \brief   convertG4ToPSEmbed()
 *
 * \param[in]    filein    input tiff file
 * \param[in]    fileout   output ps file
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This function takes a g4 compressed tif file as input and
 *          generates a g4 compressed, ascii85 encoded PS file, with
 *          a bounding box.
 *      (2) The bounding box is required when a program such as TeX
 *          (through epsf) places and rescales the image.
 *      (3) The bounding box is sized for fitting the image to an
 *          8.5 x 11.0 inch page.
 *      (4) We paint this through a mask, over whatever is below.
 * </pre>
 */
l_ok
convertG4ToPSEmbed(const char  *filein,
                   const char  *fileout)
{
char         *outstr;
l_int32       w, h, nbytes, ret;
l_float32     xpt, ypt, wpt, hpt;
L_COMP_DATA  *cid;

    if (!filein)
        return ERROR_INT("filein not defined", __func__, 1);
    if (!fileout)
        return ERROR_INT("fileout not defined", __func__, 1);

    if ((cid = l_generateG4Data(filein, 1)) == NULL)
        return ERROR_INT("g4 data not made", __func__, 1);
    w = cid->w;
    h = cid->h;

        /* Scale for 20 pt boundary and otherwise full filling
         * in one direction on 8.5 x 11 inch device */
    xpt = 20.0;
    ypt = 20.0;
    if (w * 11.0 > h * 8.5) {
        wpt = 572.0;   /* 612 - 2 * 20 */
        hpt = wpt * (l_float32)h / (l_float32)w;
    } else {
        hpt = 752.0;   /* 792 - 2 * 20 */
        wpt = hpt * (l_float32)w / (l_float32)h;
    }

        /* Generate the PS, painting through the image mask.
         * The bounding box information should be inserted (default). */
    outstr = generateG4PS(NULL, cid, xpt, ypt, wpt, hpt, 1, 1, 1);
    l_CIDataDestroy(&cid);
    if (!outstr)
        return ERROR_INT("outstr not made", __func__, 1);
    nbytes = strlen(outstr);

    ret = l_binaryWrite(fileout, "w", outstr, nbytes);
    LEPT_FREE(outstr);
    if (ret) L_ERROR("ps string not written to file\n", __func__);
    return ret;
}


/*!
 * \brief   convertG4ToPS()
 *
 * \param[in]    filein     input tiff g4 file
 * \param[in]    fileout    output ps file
 * \param[in]    operation  "w" for write; "a" for append
 * \param[in]    x, y       location of LL corner of image, in pixels, relative
 *                          to the PostScript origin (0,0) at the LL corner
 *                          of the page
 * \param[in]    res        resolution of the input image, in ppi; typ. values
 *                          are 300 and 600; use 0 for automatic determination
 *                          based on image size
 * \param[in]    scale      scaling by printer; use 0.0 or 1.0 for no scaling
 * \param[in]    pageno     page number; must start with 1; you can use 0
 *                          if there is only one page.
 * \param[in]    maskflag   boolean: use TRUE if just painting through fg;
 *                          FALSE if painting both fg and bg.
 * \param[in]    endpage    boolean: use TRUE if this is the last image to be
 *                          added to the page; FALSE otherwise
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) See the usage comments in convertJpegToPS(), some of
 *          which are repeated here.
 *      (2) This is a wrapper for tiff g4.  The PostScript that
 *          is generated is expanded by about 5/4 (due to the
 *          ascii85 encoding.  If you convert to pdf (ps2pdf), the
 *          ascii85 decoder is automatically invoked, so that the
 *          pdf wrapped g4 file is essentially the same size as
 *          the original g4 file.  It's useful to have the PS
 *          file ascii85 encoded, because many printers will not
 *          print binary PS files.
 *      (3) For the first image written to a file, use "w", which
 *          opens for write and clears the file.  For all subsequent
 *          images written to that file, use "a".
 *      (4) To render multiple images on the same page, set
 *          endpage = FALSE for each image until you get to the
 *          last, for which you set endpage = TRUE.  This causes the
 *          "showpage" command to be invoked.  Showpage outputs
 *          the entire page and clears the raster buffer for the
 *          next page to be added.  Without a "showpage",
 *          subsequent images from the next page will overlay those
 *          previously put down.
 *      (5) For multiple images to the same page, where you are writing
 *          both jpeg and tiff-g4, you have two options:
 *           (a) write the g4 first, as either image (maskflag == FALSE)
 *               or imagemask (maskflag == TRUE), and then write the
 *               jpeg over it.
 *           (b) write the jpeg first and as the last item, write
 *               the g4 as an imagemask (maskflag == TRUE), to paint
 *               through the foreground only.
 *          We have this flexibility with the tiff-g4 because it is 1 bpp.
 *      (6) For multiple pages, increment the page number, starting
 *          with page 1.  This allows PostScript (and PDF) to build
 *          a page directory, which viewers use for navigation.
 * </pre>
 */
l_ok
convertG4ToPS(const char  *filein,
              const char  *fileout,
              const char  *operation,
              l_int32      x,
              l_int32      y,
              l_int32      res,
              l_float32    scale,
              l_int32      pageno,
              l_int32      maskflag,
              l_int32      endpage)
{
char    *outstr;
l_int32  nbytes, ret;

    if (!filein)
        return ERROR_INT("filein not defined", __func__, 1);
    if (!fileout)
        return ERROR_INT("fileout not defined", __func__, 1);
    if (strcmp(operation, "w") && strcmp(operation, "a"))
        return ERROR_INT("operation must be \"w\" or \"a\"", __func__, 1);

    if (convertG4ToPSString(filein, &outstr, &nbytes, x, y, res, scale,
                            pageno, maskflag, endpage))
        return ERROR_INT("ps string not made", __func__, 1);

    ret = l_binaryWrite(fileout, operation, outstr, nbytes);
    LEPT_FREE(outstr);
    if (ret)
        return ERROR_INT("ps string not written to file", __func__, 1);
    return 0;
}


/*!
 * \brief   convertG4ToPSString()
 *
 * \param[in]    filein     input tiff g4 file
 * \param[out]   poutstr    PS string
 * \param[out]   pnbytes    number of bytes in PS string
 * \param[in]    x, y       location of LL corner of image, in pixels, relative
 *                          to the PostScript origin (0,0) at the LL corner
 *                          of the page
 * \param[in]    res        resolution of the input image, in ppi; typ. values
 *                          are 300 and 600; use 0 for automatic determination
 *                          based on image size
 * \param[in]    scale      scaling by printer; use 0.0 or 1.0 for no scaling
 * \param[in]    pageno     page number; must start with 1; you can use 0
 *                          if there is only one page.
 * \param[in]    maskflag   boolean: use TRUE if just painting through fg;
 *                          FALSE if painting both fg and bg.
 * \param[in]    endpage    boolean: use TRUE if this is the last image to be
 *                          added to the page; FALSE otherwise
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Generates PS string in G4 compressed tiff format from G4 tiff file.
 *      (2) For usage, see convertG4ToPS().
 * </pre>
 */
static l_ok
convertG4ToPSString(const char  *filein,
                    char       **poutstr,
                    l_int32     *pnbytes,
                    l_int32      x,
                    l_int32      y,
                    l_int32      res,
                    l_float32    scale,
                    l_int32      pageno,
                    l_int32      maskflag,
                    l_int32      endpage)
{
char         *outstr;
l_float32     xpt, ypt, wpt, hpt;
L_COMP_DATA  *cid;

    if (!poutstr)
        return ERROR_INT("&outstr not defined", __func__, 1);
    if (!pnbytes)
        return ERROR_INT("&nbytes not defined", __func__, 1);
    *poutstr = NULL;
    *pnbytes = 0;
    if (!filein)
        return ERROR_INT("filein not defined", __func__, 1);

    if ((cid = l_generateG4Data(filein, 1)) == NULL)
        return ERROR_INT("g4 data not made", __func__, 1);

        /* Get scaled location in pts.  Guess the input scan resolution
         * based on the input parameter %res, the resolution data in
         * the pix, and the size of the image. */
    if (scale == 0.0)
        scale = 1.0;
    if (res <= 0) {
        if (cid->res > 0) {
            res = cid->res;
        } else {
            if (cid->h <= 3509)  /* A4 height at 300 ppi */
                res = 300;
            else
                res = 600;
        }
    }
    xpt = scale * x * 72.f / res;
    ypt = scale * y * 72.f / res;
    wpt = scale * cid->w * 72.f / res;
    hpt = scale * cid->h * 72.f / res;

    if (pageno == 0)
        pageno = 1;

#if  DEBUG_G4
    lept_stderr("w = %d, h = %d, minisblack = %d\n",
                cid->w, cid->h, cid->minisblack);
    lept_stderr("comp bytes = %ld, nbytes85 = %ld\n",
                (unsigned long)cid->nbytescomp, (unsigned long)cid->nbytes85);
    lept_stderr("xpt = %7.2f, ypt = %7.2f, wpt = %7.2f, hpt = %7.2f\n",
                xpt, ypt, wpt, hpt);
#endif   /* DEBUG_G4 */

        /* Generate the PS */
    outstr = generateG4PS(NULL, cid, xpt, ypt, wpt, hpt,
                          maskflag, pageno, endpage);
    l_CIDataDestroy(&cid);
    if (!outstr)
        return ERROR_INT("outstr not made", __func__, 1);
    *poutstr = outstr;
    *pnbytes = strlen(outstr);
    return 0;
}


/*!
 * \brief   generateG4PS()
 *
 * \param[in]    filein     [optional] input tiff g4 file; can be null
 * \param[in]    cid g4     compressed image data
 * \param[in]    xpt, ypt   location of LL corner of image, in pts, relative
 *                          to the PostScript origin (0,0) at the LL corner
 *                          of the page
 * \param[in]    wpt, hpt   rendered image size in pts
 * \param[in]    maskflag   boolean: use TRUE if just painting through fg;
 *                          FALSE if painting both fg and bg.
 * \param[in]    pageno     page number; must start with 1; you can use 0
 *                          if there is only one page.
 * \param[in]    endpage    boolean: use TRUE if this is the last image to be
 *                          added to the page; FALSE otherwise
 * \return  PS string, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Low-level function.
 * </pre>
 */
static char *
generateG4PS(const char   *filein,
             L_COMP_DATA  *cid,
             l_float32     xpt,
             l_float32     ypt,
             l_float32     wpt,
             l_float32     hpt,
             l_int32       maskflag,
             l_int32       pageno,
             l_int32       endpage)
{
l_int32  w, h;
char    *outstr;
char     bigbuf[Bufsize];
SARRAY  *sa;

    if (!cid)
        return (char *)ERROR_PTR("g4 data not defined", __func__, NULL);
    w = cid->w;
    h = cid->h;

    sa = sarrayCreate(50);
    sarrayAddString(sa, "%!PS-Adobe-3.0", L_COPY);
    sarrayAddString(sa, "%%Creator: leptonica", L_COPY);
    if (filein)
        snprintf(bigbuf, sizeof(bigbuf), "%%%%Title: %s", filein);
    else
        snprintf(bigbuf, sizeof(bigbuf), "%%%%Title: G4 compressed PS");
    sarrayAddString(sa, bigbuf, L_COPY);
    sarrayAddString(sa, "%%DocumentData: Clean7Bit", L_COPY);

    if (var_PS_WRITE_BOUNDING_BOX == 1) {
        snprintf(bigbuf, sizeof(bigbuf),
            "%%%%BoundingBox: %7.2f %7.2f %7.2f %7.2f",
                    xpt, ypt, xpt + wpt, ypt + hpt);
        sarrayAddString(sa, bigbuf, L_COPY);
    }

    sarrayAddString(sa, "%%LanguageLevel: 2", L_COPY);
    sarrayAddString(sa, "%%EndComments", L_COPY);
    snprintf(bigbuf, sizeof(bigbuf), "%%%%Page: %d %d", pageno, pageno);
    sarrayAddString(sa, bigbuf, L_COPY);

    sarrayAddString(sa, "save", L_COPY);
    sarrayAddString(sa, "100 dict begin", L_COPY);

    snprintf(bigbuf, sizeof(bigbuf),
        "%7.2f %7.2f translate         %%set image origin in pts", xpt, ypt);
    sarrayAddString(sa, bigbuf, L_COPY);

    snprintf(bigbuf, sizeof(bigbuf),
        "%7.2f %7.2f scale             %%set image size in pts", wpt, hpt);
    sarrayAddString(sa, bigbuf, L_COPY);

    sarrayAddString(sa, "/DeviceGray setcolorspace", L_COPY);

    sarrayAddString(sa, "{", L_COPY);
    sarrayAddString(sa,
          "  /RawData currentfile /ASCII85Decode filter def", L_COPY);
    sarrayAddString(sa, "  << ", L_COPY);
    sarrayAddString(sa, "    /ImageType 1", L_COPY);
    snprintf(bigbuf, sizeof(bigbuf), "    /Width %d", w);
    sarrayAddString(sa, bigbuf, L_COPY);
    snprintf(bigbuf, sizeof(bigbuf), "    /Height %d", h);
    sarrayAddString(sa, bigbuf, L_COPY);
    snprintf(bigbuf, sizeof(bigbuf),
             "    /ImageMatrix [ %d 0 0 %d 0 %d ]", w, -h, h);
    sarrayAddString(sa, bigbuf, L_COPY);
    sarrayAddString(sa, "    /BitsPerComponent 1", L_COPY);
    sarrayAddString(sa, "    /Interpolate true", L_COPY);
    if (cid->minisblack)
        sarrayAddString(sa, "    /Decode [1 0]", L_COPY);
    else  /* miniswhite; typical for 1 bpp */
        sarrayAddString(sa, "    /Decode [0 1]", L_COPY);
    sarrayAddString(sa, "    /DataSource RawData", L_COPY);
    sarrayAddString(sa, "        <<", L_COPY);
    sarrayAddString(sa, "          /K -1", L_COPY);
    snprintf(bigbuf, sizeof(bigbuf), "          /Columns %d", w);
    sarrayAddString(sa, bigbuf, L_COPY);
    snprintf(bigbuf, sizeof(bigbuf), "          /Rows %d", h);
    sarrayAddString(sa, bigbuf, L_COPY);
    sarrayAddString(sa, "        >> /CCITTFaxDecode filter", L_COPY);
    if (maskflag == TRUE)  /* just paint through the fg */
        sarrayAddString(sa, "  >> imagemask", L_COPY);
    else  /* Paint full image */
        sarrayAddString(sa, "  >> image", L_COPY);
    sarrayAddString(sa, "  RawData flushfile", L_COPY);
    if (endpage == TRUE)
        sarrayAddString(sa, "  showpage", L_COPY);
    sarrayAddString(sa, "}", L_COPY);

    sarrayAddString(sa, "%%BeginData:", L_COPY);
    sarrayAddString(sa, "exec", L_COPY);

        /* Insert the ascii85 ccittg4 data; this is now owned by sa */
    sarrayAddString(sa, cid->data85, L_INSERT);

        /* Concat the trailing data */
    sarrayAddString(sa, "%%EndData", L_COPY);
    sarrayAddString(sa, "end", L_COPY);
    sarrayAddString(sa, "restore", L_COPY);

    outstr = sarrayToString(sa, 1);
    sarrayDestroy(&sa);
    cid->data85 = NULL;  /* it has been transferred and destroyed */
    return outstr;
}


/*-------------------------------------------------------------*
 *                     For tiff multipage files                *
 *-------------------------------------------------------------*/
/*!
 * \brief   convertTiffMultipageToPS()
 *
 * \param[in]    filein      input tiff multipage file
 * \param[in]    fileout     output ps file
 * \param[in]    fillfract   factor for filling 8.5 x 11 inch page;
 *                           use 0.0 for DefaultFillFraction
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This converts a multipage tiff file of binary page images
 *          into a ccitt g4 compressed PS file.
 *      (2) If the images are generated from a standard resolution fax,
 *          the vertical resolution is doubled to give a normal-looking
 *          aspect ratio.
 * </pre>
 */
l_ok
convertTiffMultipageToPS(const char  *filein,
                         const char  *fileout,
                         l_float32    fillfract)
{
char      *tempfile;
l_int32    i, npages, w, h, istiff;
l_float32  scale;
PIX       *pix, *pixs;
FILE      *fp;

    if (!filein)
        return ERROR_INT("filein not defined", __func__, 1);
    if (!fileout)
        return ERROR_INT("fileout not defined", __func__, 1);

    if ((fp = fopenReadStream(filein)) == NULL)
        return ERROR_INT_1("file not found", filein, __func__, 1);
    istiff = fileFormatIsTiff(fp);
    if (!istiff) {
        fclose(fp);
        return ERROR_INT_1("file not tiff format", filein, __func__, 1);
    }
    tiffGetCount(fp, &npages);
    fclose(fp);

    if (fillfract == 0.0)
        fillfract = DefaultFillFraction;

    for (i = 0; i < npages; i++) {
        if ((pix = pixReadTiff(filein, i)) == NULL)
            return ERROR_INT_1("pix not made", filein, __func__, 1);

        pixGetDimensions(pix, &w, &h, NULL);
        if (w == 1728 && h < w)   /* it's a std res fax */
            pixs = pixScale(pix, 1.0, 2.0);
        else
            pixs = pixClone(pix);

        tempfile = l_makeTempFilename();
        pixWrite(tempfile, pixs, IFF_TIFF_G4);
        scale = L_MIN(fillfract * 2550 / w, fillfract * 3300 / h);
        if (i == 0)
            convertG4ToPS(tempfile, fileout, "w", 0, 0, 300, scale,
                          i + 1, FALSE, TRUE);
        else
            convertG4ToPS(tempfile, fileout, "a", 0, 0, 300, scale,
                          i + 1, FALSE, TRUE);
        lept_rmfile(tempfile);
        LEPT_FREE(tempfile);
        pixDestroy(&pix);
        pixDestroy(&pixs);
    }

    return 0;
}


/*---------------------------------------------------------------------*
 *            For flate (gzip) compressed images (e.g., png)           *
 *---------------------------------------------------------------------*/
/*!
 * \brief   convertFlateToPSEmbed()
 *
 * \param[in]    filein    input file -- any format
 * \param[in]    fileout   output ps file
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This function takes any image file as input and generates a
 *          flate-compressed, ascii85 encoded PS file, with a bounding box.
 *      (2) The bounding box is required when a program such as TeX
 *          (through epsf) places and rescales the image.
 *      (3) The bounding box is sized for fitting the image to an
 *          8.5 x 11.0 inch page.
 * </pre>
 */
l_ok
convertFlateToPSEmbed(const char  *filein,
                      const char  *fileout)
{
char         *outstr;
l_int32       w, h, nbytes, ret;
l_float32     xpt, ypt, wpt, hpt;
L_COMP_DATA  *cid;

    if (!filein)
        return ERROR_INT("filein not defined", __func__, 1);
    if (!fileout)
        return ERROR_INT("fileout not defined", __func__, 1);

    if ((cid = l_generateFlateData(filein, 1)) == NULL)
        return ERROR_INT("flate data not made", __func__, 1);
    w = cid->w;
    h = cid->h;

        /* Scale for 20 pt boundary and otherwise full filling
         * in one direction on 8.5 x 11 inch device */
    xpt = 20.0;
    ypt = 20.0;
    if (w * 11.0 > h * 8.5) {
        wpt = 572.0;   /* 612 - 2 * 20 */
        hpt = wpt * (l_float32)h / (l_float32)w;
    } else {
        hpt = 752.0;   /* 792 - 2 * 20 */
        wpt = hpt * (l_float32)w / (l_float32)h;
    }

        /* Generate the PS.
         * The bounding box information should be inserted (default). */
    outstr = generateFlatePS(NULL, cid, xpt, ypt, wpt, hpt, 1, 1);
    l_CIDataDestroy(&cid);
    if (!outstr)
        return ERROR_INT("outstr not made", __func__, 1);
    nbytes = strlen(outstr);

    ret = l_binaryWrite(fileout, "w", outstr, nbytes);
    LEPT_FREE(outstr);
    if (ret) L_ERROR("ps string not written to file\n", __func__);
    return ret;
}


/*!
 * \brief   convertFlateToPS()
 *
 * \param[in]    filein    input file -- any format
 * \param[in]    fileout    output ps file
 * \param[in]    operation  "w" for write; "a" for append
 * \param[in]    x, y       location of LL corner of image, in pixels, relative
 *                          to the PostScript origin (0,0) at the LL corner
 *                          of the page
 * \param[in]    res        resolution of the input image, in ppi;
 *                          use 0 for default
 * \param[in]    scale      scaling by printer; use 0.0 or 1.0 for no scaling
 * \param[in]    pageno     page number; must start with 1; you can use 0
 *                          if there is only one page.
 * \param[in]    endpage    boolean: use TRUE if this is the last image to be
 *                          added to the page; FALSE otherwise
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This outputs level 3 PS as flate compressed (overlaid
 *          with ascii85 encoding).
 *      (2) An output file can contain multiple pages, each with
 *          multiple images.  The arguments to convertFlateToPS()
 *          allow you to control placement of png images on multiple
 *          pages within a PostScript file.
 *      (3) For the first image written to a file, use "w", which
 *          opens for write and clears the file.  For all subsequent
 *          images written to that file, use "a".
 *      (4) The (x, y) parameters give the LL corner of the image
 *          relative to the LL corner of the page.  They are in
 *          units of pixels if scale = 1.0.  If you use (e.g.)
 *          scale = 2.0, the image is placed at (2x, 2y) on the page,
 *          and the image dimensions are also doubled.
 *      (5) Display vs printed resolution:
 *           * If your display is 75 ppi and your image was created
 *             at a resolution of 300 ppi, you can get the image
 *             to print at the same size as it appears on your display
 *             by either setting scale = 4.0 or by setting  res = 75.
 *             Both tell the printer to make a 4x enlarged image.
 *           * If your image is generated at 150 ppi and you use scale = 1,
 *             it will be rendered such that 150 pixels correspond
 *             to 72 pts (1 inch on the printer).  This function does
 *             the conversion from pixels (with or without scaling) to
 *             pts, which are the units that the printer uses.
 *           * The printer will choose its own resolution to use
 *             in rendering the image, which will not affect the size
 *             of the rendered image.  That is because the output
 *             PostScript file describes the geometry in terms of pts,
 *             which are defined to be 1/72 inch.  The printer will
 *             only see the size of the image in pts, through the
 *             scale and translate parameters and the affine
 *             transform (the ImageMatrix) of the image.
 *      (6) To render multiple images on the same page, set
 *          endpage = FALSE for each image until you get to the
 *          last, for which you set endpage = TRUE.  This causes the
 *          "showpage" command to be invoked.  Showpage outputs
 *          the entire page and clears the raster buffer for the
 *          next page to be added.  Without a "showpage",
 *          subsequent images from the next page will overlay those
 *          previously put down.
 *      (7) For multiple pages, increment the page number, starting
 *          with page 1.  This allows PostScript (and PDF) to build
 *          a page directory, which viewers use for navigation.
 * </pre>
 */
l_ok
convertFlateToPS(const char  *filein,
                 const char  *fileout,
                 const char  *operation,
                 l_int32      x,
                 l_int32      y,
                 l_int32      res,
                 l_float32    scale,
                 l_int32      pageno,
                 l_int32      endpage)
{
char    *outstr;
l_int32  nbytes, ret;

    if (!filein)
        return ERROR_INT("filein not defined", __func__, 1);
    if (!fileout)
        return ERROR_INT("fileout not defined", __func__, 1);
    if (strcmp(operation, "w") && strcmp(operation, "a"))
        return ERROR_INT("operation must be \"w\" or \"a\"", __func__, 1);

    if (convertFlateToPSString(filein, &outstr, &nbytes, x, y, res, scale,
                               pageno, endpage))
        return ERROR_INT("ps string not made", __func__, 1);

    ret = l_binaryWrite(fileout, operation, outstr, nbytes);
    LEPT_FREE(outstr);
    if (ret) L_ERROR("ps string not written to file\n", __func__);
    return ret;
}


/*!
 * \brief   convertFlateToPSString()
 *
 *      Generates level 3 PS string in flate compressed format.
 *
 * \param[in]    filein    input image file
 * \param[out]   poutstr   PS string
 * \param[out]   pnbytes   number of bytes in PS string
 * \param[in]    x, y      location of LL corner of image, in pixels, relative
 *                         to the PostScript origin (0,0) at the LL corner
 *                         of the page
 * \param[in]    res       resolution of the input image, in ppi;
 *                         use 0 for default
 * \param[in]    scale     scaling by printer; use 0.0 or 1.0 for no scaling
 * \param[in]    pageno    page number; must start with 1; you can use 0
 *                         if there is only one page.
 * \param[in]    endpage   boolean: use TRUE if this is the last image to be
 *                         added to the page; FALSE otherwise
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The returned PS character array is a null-terminated
 *          ascii string.  All the raster data is ascii85 encoded, so
 *          there are no null bytes embedded in it.
 *      (2) The raster encoding is made with gzip, the same as that
 *          in a png file that is compressed without prediction.
 *          The raster data itself is 25% larger than that in the
 *          binary form, due to the ascii85 encoding.
 *
 *  Usage:  See convertFlateToPS()
 * </pre>
 */
static l_ok
convertFlateToPSString(const char  *filein,
                       char       **poutstr,
                       l_int32     *pnbytes,
                       l_int32      x,
                       l_int32      y,
                       l_int32      res,
                       l_float32    scale,
                       l_int32      pageno,
                       l_int32      endpage)
{
char         *outstr;
l_float32     xpt, ypt, wpt, hpt;
L_COMP_DATA  *cid;

    if (!poutstr)
        return ERROR_INT("&outstr not defined", __func__, 1);
    if (!pnbytes)
        return ERROR_INT("&nbytes not defined", __func__, 1);
    *pnbytes = 0;
    *poutstr = NULL;
    if (!filein)
        return ERROR_INT("filein not defined", __func__, 1);

    if ((cid = l_generateFlateData(filein, 1)) == NULL)
        return ERROR_INT("flate data not made", __func__, 1);

        /* Get scaled location in pts.  Guess the input scan resolution
         * based on the input parameter %res, the resolution data in
         * the pix, and the size of the image. */
    if (scale == 0.0)
        scale = 1.0;
    if (res <= 0) {
        if (cid->res > 0)
            res = cid->res;
        else
            res = DefaultInputRes;
    }
    xpt = scale * x * 72.f / res;
    ypt = scale * y * 72.f / res;
    wpt = scale * cid->w * 72.f / res;
    hpt = scale * cid->h * 72.f / res;

    if (pageno == 0)
        pageno = 1;

#if  DEBUG_FLATE
    lept_stderr("w = %d, h = %d, bps = %d, spp = %d\n",
                cid->w, cid->h, cid->bps, cid->spp);
    lept_stderr("uncomp bytes = %ld, comp bytes = %ld, nbytes85 = %ld\n",
                (unsigned long)cid->nbytes, (unsigned long)cid->nbytescomp,
                (unsigned long)cid->nbytes85);
    lept_stderr("xpt = %7.2f, ypt = %7.2f, wpt = %7.2f, hpt = %7.2f\n",
                xpt, ypt, wpt, hpt);
#endif   /* DEBUG_FLATE */

        /* Generate the PS */
    outstr = generateFlatePS(NULL, cid, xpt, ypt, wpt, hpt, pageno, endpage);
    l_CIDataDestroy(&cid);
    if (!outstr)
        return ERROR_INT("outstr not made", __func__, 1);
    *poutstr = outstr;
    *pnbytes = strlen(outstr);
    return 0;
}


/*!
 * \brief   generateFlatePS()
 *
 * \param[in]    filein      [optional] input filename; can be null
 * \param[in]    cid         flate compressed image data
 * \param[in]    xpt, ypt    location of LL corner of image, in pts, relative
 *                           to the PostScript origin (0,0) at the LL corner
 *                           of the page
 * \param[in]    wpt, hpt    rendered image size in pts
 * \param[in]    pageno      page number; must start with 1; you can use 0
 *                           if there is only one page
 * \param[in]    endpage     boolean: use TRUE if this is the last image to be
 *                           added to the page; FALSE otherwise
 * \return  PS string, or NULL on error
 */
static char *
generateFlatePS(const char   *filein,
                L_COMP_DATA  *cid,
                l_float32     xpt,
                l_float32     ypt,
                l_float32     wpt,
                l_float32     hpt,
                l_int32       pageno,
                l_int32       endpage)
{
l_int32  w, h, bps, spp;
char    *outstr;
char     bigbuf[Bufsize];
SARRAY  *sa;

    if (!cid)
        return (char *)ERROR_PTR("flate data not defined", __func__, NULL);
    w = cid->w;
    h = cid->h;
    bps = cid->bps;
    spp = cid->spp;

    sa = sarrayCreate(50);
    sarrayAddString(sa, "%!PS-Adobe-3.0 EPSF-3.0", L_COPY);
    sarrayAddString(sa, "%%Creator: leptonica", L_COPY);
    if (filein)
        snprintf(bigbuf, sizeof(bigbuf), "%%%%Title: %s", filein);
    else
        snprintf(bigbuf, sizeof(bigbuf), "%%%%Title: Flate compressed PS");
    sarrayAddString(sa, bigbuf, L_COPY);
    sarrayAddString(sa, "%%DocumentData: Clean7Bit", L_COPY);

    if (var_PS_WRITE_BOUNDING_BOX == 1) {
        snprintf(bigbuf, sizeof(bigbuf),
                 "%%%%BoundingBox: %7.2f %7.2f %7.2f %7.2f",
                 xpt, ypt, xpt + wpt, ypt + hpt);
        sarrayAddString(sa, bigbuf, L_COPY);
    }

    sarrayAddString(sa, "%%LanguageLevel: 3", L_COPY);
    sarrayAddString(sa, "%%EndComments", L_COPY);
    snprintf(bigbuf, sizeof(bigbuf), "%%%%Page: %d %d", pageno, pageno);
    sarrayAddString(sa, bigbuf, L_COPY);

    sarrayAddString(sa, "save", L_COPY);
    snprintf(bigbuf, sizeof(bigbuf),
           "%7.2f %7.2f translate         %%set image origin in pts", xpt, ypt);
    sarrayAddString(sa, bigbuf, L_COPY);

    snprintf(bigbuf, sizeof(bigbuf),
             "%7.2f %7.2f scale             %%set image size in pts", wpt, hpt);
    sarrayAddString(sa, bigbuf, L_COPY);

        /* If there is a colormap, add the data; it is now owned by sa */
    if (cid->cmapdata85) {
        snprintf(bigbuf, sizeof(bigbuf),
                 "[ /Indexed /DeviceRGB %d          %%set colormap type/size",
                 cid->ncolors - 1);
        sarrayAddString(sa, bigbuf, L_COPY);
        sarrayAddString(sa, "  <~", L_COPY);
        sarrayAddString(sa, cid->cmapdata85, L_INSERT);
        sarrayAddString(sa, "  ] setcolorspace", L_COPY);
    } else if (spp == 1) {
        sarrayAddString(sa, "/DeviceGray setcolorspace", L_COPY);
    } else {  /* spp == 3 */
        sarrayAddString(sa, "/DeviceRGB setcolorspace", L_COPY);
    }

    sarrayAddString(sa,
                    "/RawData currentfile /ASCII85Decode filter def", L_COPY);
    sarrayAddString(sa,
                    "/Data RawData << >> /FlateDecode filter def", L_COPY);

    sarrayAddString(sa, "{ << /ImageType 1", L_COPY);
    snprintf(bigbuf, sizeof(bigbuf), "     /Width %d", w);
    sarrayAddString(sa, bigbuf, L_COPY);
    snprintf(bigbuf, sizeof(bigbuf), "     /Height %d", h);
    sarrayAddString(sa, bigbuf, L_COPY);
    snprintf(bigbuf, sizeof(bigbuf), "     /BitsPerComponent %d", bps);
    sarrayAddString(sa, bigbuf, L_COPY);
    snprintf(bigbuf, sizeof(bigbuf),
            "     /ImageMatrix [ %d 0 0 %d 0 %d ]", w, -h, h);
    sarrayAddString(sa, bigbuf, L_COPY);

    if (cid->cmapdata85) {
        sarrayAddString(sa, "     /Decode [0 255]", L_COPY);
    } else if (spp == 1) {
        if (bps == 1)  /* miniswhite photometry */
            sarrayAddString(sa, "     /Decode [1 0]", L_COPY);
        else  /* bps > 1 */
            sarrayAddString(sa, "     /Decode [0 1]", L_COPY);
    } else {  /* spp == 3 */
        sarrayAddString(sa, "     /Decode [0 1 0 1 0 1]", L_COPY);
    }

    sarrayAddString(sa, "     /DataSource Data", L_COPY);
    sarrayAddString(sa, "  >> image", L_COPY);
    sarrayAddString(sa, "  Data closefile", L_COPY);
    sarrayAddString(sa, "  RawData flushfile", L_COPY);
    if (endpage == TRUE)
        sarrayAddString(sa, "  showpage", L_COPY);
    sarrayAddString(sa, "  restore", L_COPY);
    sarrayAddString(sa, "} exec", L_COPY);

        /* Insert the ascii85 gzipped data; this is now owned by sa */
    sarrayAddString(sa, cid->data85, L_INSERT);

        /* Generate and return the output string */
    outstr = sarrayToString(sa, 1);
    sarrayDestroy(&sa);
    cid->cmapdata85 = NULL;  /* it has been transferred to sa and destroyed */
    cid->data85 = NULL;  /* it has been transferred to sa and destroyed */
    return outstr;
}


/*---------------------------------------------------------------------*
 *                          Write to memory                            *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixWriteMemPS()
 *
 * \param[out]   pdata    data of tiff compressed image
 * \param[out]   psize    size of returned data
 * \param[in]    pix
 * \param[in]    box      [optional]
 * \param[in]    res      can use 0 for default of 300 ppi
 * \param[in]    scale    to prevent scaling, use either 1.0 or 0.0
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) See pixWriteStringPS() for usage.
 *      (2) This is just a wrapper for pixWriteStringPS(), which
 *          writes uncompressed image data to memory.
 * </pre>
 */
l_ok
pixWriteMemPS(l_uint8  **pdata,
              size_t    *psize,
              PIX       *pix,
              BOX       *box,
              l_int32    res,
              l_float32  scale)
{
    if (!pdata)
        return ERROR_INT("&data not defined", __func__, 1 );
    if (!psize)
        return ERROR_INT("&size not defined", __func__, 1 );
    if (!pix)
        return ERROR_INT("&pix not defined", __func__, 1 );

    *pdata = (l_uint8 *)pixWriteStringPS(pix, box, res, scale);
    *psize = strlen((char *)(*pdata));
    return 0;
}


/*-------------------------------------------------------------*
 *                    Converting resolution                    *
 *-------------------------------------------------------------*/
/*!
 * \brief   getResLetterPage()
 *
 * \param[in]    w           image width, pixels
 * \param[in]    h           image height, pixels
 * \param[in]    fillfract   fraction in linear dimension of full page,
 *                           not to be exceeded; use 0 for default
 * \return  resolution
 */
l_int32
getResLetterPage(l_int32    w,
                 l_int32    h,
                 l_float32  fillfract)
{
l_int32  resw, resh, res;

    if (fillfract == 0.0)
        fillfract = DefaultFillFraction;
    resw = (l_int32)((w * 72.) / (LetterWidth * fillfract));
    resh = (l_int32)((h * 72.) / (LetterHeight * fillfract));
    res = L_MAX(resw, resh);
    return res;
}


/*!
 * \brief   getResA4Page()
 *
 * \param[in]    w           image width, pixels
 * \param[in]    h           image height, pixels
 * \param[in]    fillfract   fraction in linear dimension of full page,
 *                           not to be exceeded; use 0 for default
 * \return  resolution
 */
l_int32
getResA4Page(l_int32    w,
             l_int32    h,
             l_float32  fillfract)
{
l_int32  resw, resh, res;

    if (fillfract == 0.0)
        fillfract = DefaultFillFraction;
    resw = (l_int32)((w * 72.) / (A4Width * fillfract));
    resh = (l_int32)((h * 72.) / (A4Height * fillfract));
    res = L_MAX(resw, resh);
    return res;
}


/*-------------------------------------------------------------*
 *           Setting flag for writing bounding box hint        *
 *-------------------------------------------------------------*/
void
l_psWriteBoundingBox(l_int32  flag)
{
    var_PS_WRITE_BOUNDING_BOX = flag;
}


/* --------------------------------------------*/
#endif  /* USE_PSIO */
/* --------------------------------------------*/
