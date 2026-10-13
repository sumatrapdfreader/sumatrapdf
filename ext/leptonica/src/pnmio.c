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
 * \file pnmio.c
 * <pre>
 *
 *      Stream interface
 *          PIX             *pixReadStreamPnm()
 *          l_int32          readHeaderPnm()
 *          l_int32          freadHeaderPnm()
 *          l_int32          pixWriteStreamPnm()
 *          l_int32          pixWriteStreamAsciiPnm()
 *          l_int32          pixWriteStreamPam()
 *
 *      Read/write to memory
 *          PIX             *pixReadMemPnm()
 *          l_int32          readHeaderMemPnm()
 *          l_int32          pixWriteMemPnm()
 *          l_int32          pixWriteMemPam()
 *
 *      Local helpers
 *          static l_int32   pnmReadNextAsciiValue();
 *          static l_int32   pnmReadNextNumber();
 *          static l_int32   pnmReadNextString();
 *          static l_int32   pnmSkipCommentLines();
 *
 *      These are here by popular demand, with the help of Mattias
 *      Kregert (mattias@kregert.se), who provided the first implementation.
 *
 *      The pnm formats are exceedingly simple, because they have
 *      no compression and no colormaps.  They support images that
 *      are 1 bpp; 2, 4, 8 and 16 bpp grayscale; and rgb.
 *
 *      The original pnm formats ("ASCII") are included for completeness,
 *      but their use is deprecated for all but tiny iconic images.
 *      They are extremely wasteful of memory; for example, the P1 binary
 *      ASCII format is 16 times as big as the packed uncompressed
 *      format, because 2 characters are used to represent every bit
 *      (pixel) in the image.  Reading is slow because we check for extra
 *      white space and EOL at every sample value.
 *
 *      The packed pnm formats ("raw") give file sizes similar to
 *      bmp files, which are uncompressed packed.  However, bmp
 *      are more flexible, because they can support colormaps.
 *
 *      We don't differentiate between the different types ("pbm",
 *      "pgm", "ppm") at the interface level, because this is really a
 *      "distinction without a difference."  You read a file, you get
 *      the appropriate Pix.  You write a file from a Pix, you get the
 *      appropriate type of file.  If there is a colormap on the Pix,
 *      and the Pix is more than 1 bpp, you get either an 8 bpp pgm
 *      or a 24 bpp RGB pnm, depending on whether the colormap colors
 *      are gray or rgb, respectively.
 *
 *      This follows the general policy that the I/O routines don't
 *      make decisions about the content of the image -- you do that
 *      with image processing before you write it out to file.
 *      The I/O routines just try to make the closest connection
 *      possible between the file and the Pix in memory.
 *
 *      On systems like Windows without fmemopen() and open_memstream(),
 *      we write data to a temp file and read it back for operations
 *      between pix and compressed-data, such as pixReadMemPnm() and
 *      pixWriteMemPnm().
 *
 *      The P7 format is new. It introduced a header with multiple
 *      lines containing distinct tags for the various fields.
 *      See: http://netpbm.sourceforge.net/doc/pam.html
 *
 *        WIDTH <int>         ; mandatory, exactly once
 *        HEIGHT <int>        ; mandatory, exactly once
 *        DEPTH <int>         ; mandatory, exactly once,
 *                            ; its meaning is equivalent to spp
 *        MAXVAL <int>        ; mandatory, one of 1, 3, 15, 255 or 65535
 *        TUPLTYPE <string>   ; optional; BLACKANDWHITE, GRAYSCALE, RGB
 *                            ; and optional suffix _ALPHA, e.g. RGB_ALPHA
 *        ENDHDR              ; mandatory, last header line
 *
 *      Reading BLACKANDWHITE_ALPHA and GRAYSCALE_ALPHA, which have a DEPTH
 *      value of 2, is supported. The original image is converted to a Pix
 *      with 32-bpp and alpha channel (spp == 4).
 *
 *      Writing P7 format is currently selected for 32-bpp with alpha
 *      channel, i.e. for Pix which have spp == 4, using pixWriteStreamPam().
 *
 *      Jürgen Buchmüller provided the implementation for the P7 (pam) format.
 *
 *      Giulio Lunati made an elegant reimplementation of the static helper
 *      functions using fscanf() instead of fseek(), so that it works with
 *      pnm data from stdin.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include <ctype.h>
#include "allheaders.h"

/* --------------------------------------------*/
#if  USE_PNMIO   /* defined in environ.h */
/* --------------------------------------------*/

static l_int32 pnmReadNextAsciiValue(FILE  *fp, l_int32 *pval);
static l_int32 pnmReadNextNumber(FILE *fp, l_int32 *pval);
static l_int32 pnmReadNextString(FILE *fp, char *buff, l_int32 size);
static l_int32 pnmSkipCommentLines(FILE  *fp);

    /* a sanity check on the size read from file */
static const l_int32  MAX_PNM_WIDTH = 100000;
static const l_int32  MAX_PNM_HEIGHT = 100000;


/*--------------------------------------------------------------------*
 *                          Stream interface                          *
 *--------------------------------------------------------------------*/
/*!
 * \brief   pixReadStreamPnm()
 *
 * \param[in]    fp   file stream opened for read
 * \return  pix, or NULL on error
 */
PIX *
pixReadStreamPnm(FILE  *fp)
{
l_uint8    val8, rval8, gval8, bval8, aval8, mask8;
l_uint16   val16, rval16, gval16, bval16, aval16;
l_int32    w, h, d, bps, spp, bpl, wpl, i, j, type;
l_int32    val, rval, gval, bval;
l_uint32   rgbval;
l_uint32  *line, *data;
PIX       *pix;

    if (!fp)
        return (PIX *)ERROR_PTR("fp not defined", __func__, NULL);

    if (freadHeaderPnm(fp, &w, &h, &d, &type, &bps, &spp))
        return (PIX *)ERROR_PTR("header read failed", __func__, NULL);
    if (bps < 1 || bps > 16)
        return (PIX *)ERROR_PTR("invalid bps", __func__, NULL);
    if (spp < 1 || spp > 4)
        return (PIX *)ERROR_PTR("invalid spp", __func__, NULL);
    if ((pix = pixCreate(w, h, d)) == NULL)
        return (PIX *)ERROR_PTR("pix not made", __func__, NULL);
    pixSetInputFormat(pix, IFF_PNM);
    data = pixGetData(pix);
    wpl = pixGetWpl(pix);

        /* If type == 6 and bps == 16, we use the code in type 7
         * to read 6 bytes/pixel from the input file. */
    if (type == 6 && bps == 16)
        type = 7;

    switch (type) {
    case 1:
    case 2:
        /* Old "ASCII" binary or gray format */
        for (i = 0; i < h; i++) {
            for (j = 0; j < w; j++) {
                if (pnmReadNextAsciiValue(fp, &val)) {
                    pixDestroy(&pix);
                    return (PIX *)ERROR_PTR("read abend", __func__, NULL);
                }
                pixSetPixel(pix, j, i, val);
            }
        }
        break;

    case 3:
        /* Old "ASCII" rgb format */
        for (i = 0; i < h; i++) {
            for (j = 0; j < w; j++) {
                if (pnmReadNextAsciiValue(fp, &rval)) {
                    pixDestroy(&pix);
                    return (PIX *)ERROR_PTR("read abend", __func__, NULL);
                }
                if (pnmReadNextAsciiValue(fp, &gval)) {
                    pixDestroy(&pix);
                    return (PIX *)ERROR_PTR("read abend", __func__, NULL);
                }
                if (pnmReadNextAsciiValue(fp, &bval)) {
                    pixDestroy(&pix);
                    return (PIX *)ERROR_PTR("read abend", __func__, NULL);
                }
                composeRGBPixel(rval, gval, bval, &rgbval);
                pixSetPixel(pix, j, i, rgbval);
            }
        }
        break;

    case 4:
        /* "raw" format for 1 bpp */
        bpl = (d * w + 7) / 8;
        for (i = 0; i < h; i++) {
            line = data + i * wpl;
            for (j = 0; j < bpl; j++) {
                if (fread(&val8, 1, 1, fp) != 1) {
                    pixDestroy(&pix);
                    return (PIX *)ERROR_PTR("read error in 4", __func__, NULL);
                }
                SET_DATA_BYTE(line, j, val8);
            }
        }
        break;

    case 5:
        /* "raw" format for grayscale */
        for (i = 0; i < h; i++) {
            line = data + i * wpl;
            if (d != 16) {
                for (j = 0; j < w; j++) {
                    if (fread(&val8, 1, 1, fp) != 1) {
                        pixDestroy(&pix);
                        return (PIX *)ERROR_PTR("error in 5", __func__, NULL);
                    }
                    if (d == 2)
                        SET_DATA_DIBIT(line, j, val8);
                    else if (d == 4)
                        SET_DATA_QBIT(line, j, val8);
                    else  /* d == 8 */
                        SET_DATA_BYTE(line, j, val8);
                }
            } else {  /* d == 16 */
                for (j = 0; j < w; j++) {
                    if (fread(&val16, 2, 1, fp) != 1) {
                        pixDestroy(&pix);
                        return (PIX *)ERROR_PTR("16 bpp error", __func__, NULL);
                    }
                    SET_DATA_TWO_BYTES(line, j, val16);
                }
            }
        }
        break;

    case 6:
        /* "raw" format, type == 6; 8 bps, rgb */
        for (i = 0; i < h; i++) {
            line = data + i * wpl;
            for (j = 0; j < wpl; j++) {
                if (fread(&rval8, 1, 1, fp) != 1) {
                    pixDestroy(&pix);
                    return (PIX *)ERROR_PTR("read error type 6",
                                            __func__, NULL);
                }
                if (fread(&gval8, 1, 1, fp) != 1) {
                    pixDestroy(&pix);
                    return (PIX *)ERROR_PTR("read error type 6",
                                            __func__, NULL);
                }
                if (fread(&bval8, 1, 1, fp) != 1) {
                    pixDestroy(&pix);
                    return (PIX *)ERROR_PTR("read error type 6",
                                            __func__, NULL);
                }
                composeRGBPixel(rval8, gval8, bval8, &rgbval);
                line[j] = rgbval;
            }
        }
        break;

    case 7:
        /* "arbitrary" format; type == 7; */
        if (bps != 16) {
            mask8 = (1 << bps) - 1;
            switch (spp) {
            case 1: /* 1, 2, 4, 8 bpp grayscale */
                for (i = 0; i < h; i++) {
                    for (j = 0; j < w; j++) {
                        if (fread(&val8, 1, 1, fp) != 1) {
                            pixDestroy(&pix);
                            return (PIX *)ERROR_PTR("read error type 7",
                                                    __func__, NULL);
                        }
                        val8 = val8 & mask8;
                        if (bps == 1) val8 ^= 1;  /* white-is-1 photometry */
                        pixSetPixel(pix, j, i, val8);
                    }
                }
                break;

            case 2: /* 1, 2, 4, 8 bpp grayscale + alpha */
                for (i = 0; i < h; i++) {
                    for (j = 0; j < w; j++) {
                        if (fread(&val8, 1, 1, fp) != 1) {
                            pixDestroy(&pix);
                            return (PIX *)ERROR_PTR("read error type 7",
                                                    __func__, NULL);
                        }
                        if (fread(&aval8, 1, 1, fp) != 1) {
                            pixDestroy(&pix);
                            return (PIX *)ERROR_PTR("read error type 7",
                                                    __func__, NULL);
                        }
                        val8 = val8 & mask8;
                        aval8 = aval8 & mask8;
                        composeRGBAPixel(val8, val8, val8, aval8, &rgbval);
                        pixSetPixel(pix, j, i, rgbval);
                    }
                }
                pixSetSpp(pix, 4);
                break;

            case 3: /* rgb */
                for (i = 0; i < h; i++) {
                    line = data + i * wpl;
                    for (j = 0; j < wpl; j++) {
                        if (fread(&rval8, 1, 1, fp) != 1) {
                            pixDestroy(&pix);
                            return (PIX *)ERROR_PTR("read error type 7",
                                                    __func__, NULL);
                        }
                        if (fread(&gval8, 1, 1, fp) != 1) {
                            pixDestroy(&pix);
                            return (PIX *)ERROR_PTR("read error type 7",
                                                    __func__, NULL);
                        }
                        if (fread(&bval8, 1, 1, fp) != 1) {
                            pixDestroy(&pix);
                            return (PIX *)ERROR_PTR("read error type 7",
                                                    __func__, NULL);
                        }
                        rval8 = rval8 & mask8;
                        gval8 = gval8 & mask8;
                        bval8 = bval8 & mask8;
                        composeRGBPixel(rval8, gval8, bval8, &rgbval);
                        line[j] = rgbval;
                    }
                }
                break;

            case 4: /* rgba */
                for (i = 0; i < h; i++) {
                    line = data + i * wpl;
                    for (j = 0; j < wpl; j++) {
                        if (fread(&rval8, 1, 1, fp) != 1) {
                            pixDestroy(&pix);
                            return (PIX *)ERROR_PTR("read error type 7",
                                                    __func__, NULL);
                        }
                        if (fread(&gval8, 1, 1, fp) != 1) {
                            pixDestroy(&pix);
                            return (PIX *)ERROR_PTR("read error type 7",
                                                    __func__, NULL);
                        }
                        if (fread(&bval8, 1, 1, fp) != 1) {
                            pixDestroy(&pix);
                            return (PIX *)ERROR_PTR("read error type 7",
                                                    __func__, NULL);
                        }
                        if (fread(&aval8, 1, 1, fp) != 1) {
                            pixDestroy(&pix);
                            return (PIX *)ERROR_PTR("read error type 7",
                                                    __func__, NULL);
                        }
                        rval8 = rval8 & mask8;
                        gval8 = gval8 & mask8;
                        bval8 = bval8 & mask8;
                        aval8 = aval8 & mask8;
                        composeRGBAPixel(rval8, gval8, bval8, aval8, &rgbval);
                        line[j] = rgbval;
                    }
                }
                pixSetSpp(pix, 4);
                break;
            }
        } else {  /* bps == 16 */
                /* I have only seen one example that is type 6, 16 bps.
                 * It was 3 spp (rgb), and the 8 bps of real data was stored
                 * in the second byte.  In the following, I make the wild
                 * assumption that for all 16 bpp pnm/pam files, we can
                 * take the second byte. */
            switch (spp) {
            case 1: /* 16 bps grayscale */
                for (i = 0; i < h; i++) {
                    for (j = 0; j < w; j++) {
                        if (fread(&val16, 2, 1, fp) != 1) {
                            pixDestroy(&pix);
                            return (PIX *)ERROR_PTR("read error type 7",
                                                    __func__, NULL);
                        }
                        val8 = val16 & 0xff;
                        pixSetPixel(pix, j, i, val8);
                    }
                }
                break;

            case 2: /* 16 bps grayscale + alpha */
                for (i = 0; i < h; i++) {
                    for (j = 0; j < w; j++) {
                        if (fread(&val16, 2, 1, fp) != 1) {
                            pixDestroy(&pix);
                            return (PIX *)ERROR_PTR("read error type 7",
                                                    __func__, NULL);
                        }
                        if (fread(&aval16, 2, 1, fp) != 1) {
                            pixDestroy(&pix);
                            return (PIX *)ERROR_PTR("read error type 7",
                                                    __func__, NULL);
                        }
                        val8 = val16 & 0xff;
                        aval8 = aval16 & 0xff;
                        composeRGBAPixel(val8, val8, val8, aval8, &rgbval);
                        pixSetPixel(pix, j, i, rgbval);
                    }
                }
                pixSetSpp(pix, 4);
                break;

            case 3: /* 16bps rgb */
                for (i = 0; i < h; i++) {
                    line = data + i * wpl;
                    for (j = 0; j < wpl; j++) {
                        if (fread(&rval16, 2, 1, fp) != 1) {
                            pixDestroy(&pix);
                            return (PIX *)ERROR_PTR("read error type 7",
                                                    __func__, NULL);
                        }
                        if (fread(&gval16, 2, 1, fp) != 1) {
                            pixDestroy(&pix);
                            return (PIX *)ERROR_PTR("read error type 7",
                                                    __func__, NULL);
                        }
                        if (fread(&bval16, 2, 1, fp) != 1) {
                            pixDestroy(&pix);
                            return (PIX *)ERROR_PTR("read error type 7",
                                                    __func__, NULL);
                        }
                        rval8 = rval16 & 0xff;
                        gval8 = gval16 & 0xff;
                        bval8 = bval16 & 0xff;
                        composeRGBPixel(rval8, gval8, bval8, &rgbval);
                        line[j] = rgbval;
                    }
                }
                break;

            case 4: /* 16bps rgba */
                for (i = 0; i < h; i++) {
                    line = data + i * wpl;
                    for (j = 0; j < wpl; j++) {
                        if (fread(&rval16, 2, 1, fp) != 1) {
                            pixDestroy(&pix);
                            return (PIX *)ERROR_PTR("read error type 7",
                                                    __func__, NULL);
                        }
                        if (fread(&gval16, 2, 1, fp) != 1) {
                            pixDestroy(&pix);
                            return (PIX *)ERROR_PTR("read error type 7",
                                                    __func__, NULL);
                        }
                        if (fread(&bval16, 2, 1, fp) != 1) {
                            pixDestroy(&pix);
                            return (PIX *)ERROR_PTR("read error type 7",
                                                    __func__, NULL);
                        }
                        if (fread(&aval16, 2, 1, fp) != 1) {
                            pixDestroy(&pix);
                            return (PIX *)ERROR_PTR("read error type 7",
                                                    __func__, NULL);
                        }
                        rval8 = rval16 & 0xff;
                        gval8 = gval16 & 0xff;
                        bval8 = bval16 & 0xff;
                        aval8 = aval16 & 0xff;
                        composeRGBAPixel(rval8, gval8, bval8, aval8, &rgbval);
                        line[j] = rgbval;
                    }
                }
                pixSetSpp(pix, 4);
                break;
            }
        }
        break;
    }
    return pix;
}


/*!
 * \brief   readHeaderPnm()
 *
 * \param[in]    filename
 * \param[out]   pw       [optional]
 * \param[out]   ph       [optional]
 * \param[out]   pd       [optional]
 * \param[out]   ptype    [optional] pnm type
 * \param[out]   pbps     [optional] bits/sample
 * \param[out]   pspp     [optional] samples/pixel
 * \return  0 if OK, 1 on error
 */
l_ok
readHeaderPnm(const char *filename,
              l_int32    *pw,
              l_int32    *ph,
              l_int32    *pd,
              l_int32    *ptype,
              l_int32    *pbps,
              l_int32    *pspp)
{
l_int32  ret;
FILE    *fp;

    if (pw) *pw = 0;
    if (ph) *ph = 0;
    if (pd) *pd = 0;
    if (ptype) *ptype = 0;
    if (pbps) *pbps = 0;
    if (pspp) *pspp = 0;
    if (!filename)
        return ERROR_INT("filename not defined", __func__, 1);

    if ((fp = fopenReadStream(filename)) == NULL)
        return ERROR_INT_1("image file not found", filename, __func__, 1);
    ret = freadHeaderPnm(fp, pw, ph, pd, ptype, pbps, pspp);
    fclose(fp);
    return ret;
}


/*!
 * \brief   freadHeaderPnm()
 *
 * \param[in]    fp     file stream opened for read
 * \param[out]   pw     [optional]
 * \param[out]   ph     [optional]
 * \param[out]   pd     [optional]
 * \param[out]   ptype  [optional] pnm type
 * \param[out]   pbps   [optional]  bits/sample
 * \param[out]   pspp   [optional]  samples/pixel
 * \return  0 if OK, 1 on error
 */
l_ok
freadHeaderPnm(FILE     *fp,
               l_int32  *pw,
               l_int32  *ph,
               l_int32  *pd,
               l_int32  *ptype,
               l_int32  *pbps,
               l_int32  *pspp)
{
char     tag[16], tupltype[32];
l_int32  i, w, h, d, bps, spp, type;
l_int32  maxval;
l_int32  ch;

    if (pw) *pw = 0;
    if (ph) *ph = 0;
    if (pd) *pd = 0;
    if (ptype) *ptype = 0;
    if (pbps) *pbps = 0;
    if (pspp) *pspp = 0;
    if (!fp)
        return ERROR_INT("fp not defined", __func__, 1);

    if (fscanf(fp, "P%d\n", &type) != 1)
        return ERROR_INT("invalid read for type", __func__, 1);
    if (type < 1 || type > 7)
        return ERROR_INT("invalid pnm file", __func__, 1);

    if (pnmSkipCommentLines(fp))
        return ERROR_INT("no data in file", __func__, 1);

    if (type == 7) {
        w = h = d = bps = spp = maxval = 0;
        for (i = 0; i < 10; i++) {   /* limit to 10 lines of this header */
            if (pnmReadNextString(fp, tag, sizeof(tag)))
                return ERROR_INT("found no next tag", __func__, 1);
            if (!strcmp(tag, "WIDTH")) {
                if (pnmReadNextNumber(fp, &w))
                    return ERROR_INT("failed reading width", __func__, 1);
                continue;
            }
            if (!strcmp(tag, "HEIGHT")) {
                if (pnmReadNextNumber(fp, &h))
                    return ERROR_INT("failed reading height", __func__, 1);
                continue;
            }
            if (!strcmp(tag, "DEPTH")) {
                if (pnmReadNextNumber(fp, &spp))
                    return ERROR_INT("failed reading depth", __func__, 1);
                continue;
            }
            if (!strcmp(tag, "MAXVAL")) {
                if (pnmReadNextNumber(fp, &maxval))
                    return ERROR_INT("failed reading maxval", __func__, 1);
                continue;
            }
            if (!strcmp(tag, "TUPLTYPE")) {
                if (pnmReadNextString(fp, tupltype, sizeof(tupltype)))
                    return ERROR_INT("failed reading tuple type", __func__, 1);
                continue;
            }
            if (!strcmp(tag, "ENDHDR")) {
                if ('\n' != (ch = fgetc(fp)))
                    return ERROR_INT("missing LF after ENDHDR", __func__, 1);
                break;
            }
        }
        if (w <= 0 || h <= 0 || w > MAX_PNM_WIDTH || h > MAX_PNM_HEIGHT) {
            L_INFO("invalid size: w = %d, h = %d\n", __func__, w, h);
            return 1;
        }
        if (maxval == 1) {
            d = bps = 1;
        } else if (maxval == 3) {
            d = bps = 2;
        } else if (maxval == 15) {
            d = bps = 4;
        } else if (maxval == 255) {
            d = bps = 8;
        } else if (maxval == 0xffff) {
            d = bps = 16;
        } else {
            L_INFO("invalid maxval = %d\n", __func__, maxval);
            return 1;
        }
        switch (spp) {
        case 1:
            /* d and bps are already set */
            break;
        case 2:
        case 3:
        case 4:
            /* create a 32 bpp Pix */
            d = 32;
            break;
        default:
            L_INFO("invalid depth = %d\n", __func__, spp);
            return 1;
        }
    } else {

        if (fscanf(fp, "%d %d\n", &w, &h) != 2)
            return ERROR_INT("invalid read for w,h", __func__, 1);
        if (w <= 0 || h <= 0 || w > MAX_PNM_WIDTH || h > MAX_PNM_HEIGHT) {
            L_INFO("invalid size: w = %d, h = %d\n", __func__, w, h);
            return 1;
        }

       /* Get depth of pix.  For types 2 and 5, we use the maxval.
        * Important implementation note:
        *   - You can't use fscanf(), which throws away whitespace,
        *     and will discard binary data if it starts with whitespace(s).
        *   - You can't use fgets(), which stops at newlines, but this
        *     dumb format doesn't require a newline after the maxval
        *     number -- it just requires one whitespace character.
        *   - Which leaves repeated calls to fgetc, including swallowing
        *     the single whitespace character. */
        if (type == 1 || type == 4) {
            d = 1;
            spp = 1;
            bps = 1;
        } else if (type == 2 || type == 5) {
            if (pnmReadNextNumber(fp, &maxval))
                return ERROR_INT("invalid read for maxval (2,5)", __func__, 1);
            if (maxval == 3) {
                d = 2;
            } else if (maxval == 15) {
                d = 4;
            } else if (maxval == 255) {
                d = 8;
            } else if (maxval == 0xffff) {
                d = 16;
            } else {
                lept_stderr("maxval = %d\n", maxval);
                return ERROR_INT("invalid maxval", __func__, 1);
            }
            bps = d;
            spp = 1;
        } else {  /* type == 3 || type == 6; this is rgb  */
            if (pnmReadNextNumber(fp, &maxval))
                return ERROR_INT("invalid read for maxval (3,6)", __func__, 1);
            if (maxval != 255 && maxval != 0xffff) {
                L_ERROR("unexpected maxval = %d\n", __func__, maxval);
                return 1;
            }
            bps = (maxval == 255) ? 8 : 16;
            d = 32;
            spp = 3;
        }
    }
    if (pw) *pw = w;
    if (ph) *ph = h;
    if (pd) *pd = d;
    if (ptype) *ptype = type;
    if (pbps) *pbps = bps;
    if (pspp) *pspp = spp;
    return 0;
}


/*!
 * \brief   pixWriteStreamPnm()
 *
 * \param[in]   fp    file stream opened for write
 * \param[in]   pix
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This writes "raw" packed format only:
 *          1 bpp --> pbm (P4)
 *          2, 4, 8, 16 bpp, no colormap or grayscale colormap --> pgm (P5)
 *          2, 4, 8 bpp with color-valued colormap, or rgb --> rgb ppm (P6)
 *      (2) 24 bpp rgb are not supported in leptonica, but this will
 *          write them out as a packed array of bytes (3 to a pixel).
 * </pre>
 */
l_ok
pixWriteStreamPnm(FILE  *fp,
                  PIX   *pix)
{
l_uint8    val8;
l_uint8    pel[4];
l_uint16   val16;
l_int32    h, w, d, ds, i, j, wpls, bpl, filebpl, writeerror, maxval;
l_uint32  *pword, *datas, *lines;
PIX       *pixs;

    if (!fp)
        return ERROR_INT("fp not defined", __func__, 1);
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    pixGetDimensions(pix, &w, &h, &d);
    if (d != 1 && d != 2 && d != 4 && d != 8 && d != 16 && d != 24 && d != 32)
        return ERROR_INT("d not in {1,2,4,8,16,24,32}", __func__, 1);
    if (d == 32 && pixGetSpp(pix) == 4)
        return pixWriteStreamPam(fp, pix);

        /* If a colormap exists, remove and convert to grayscale or rgb */
    if (pixGetColormap(pix) != NULL)
        pixs = pixRemoveColormap(pix, REMOVE_CMAP_BASED_ON_SRC);
    else
        pixs = pixClone(pix);
    ds =  pixGetDepth(pixs);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);

    writeerror = 0;

    if (ds == 1) {  /* binary */
        fprintf(fp, "P4\n# Raw PBM file written by leptonica "
                    "(www.leptonica.com)\n%d %d\n", w, h);

        bpl = (w + 7) / 8;
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            for (j = 0; j < bpl; j++) {
                val8 = GET_DATA_BYTE(lines, j);
                fwrite(&val8, 1, 1, fp);
            }
        }
    } else if (ds == 2 || ds == 4 || ds == 8 || ds == 16) {  /* grayscale */
        maxval = (1 << ds) - 1;
        fprintf(fp, "P5\n# Raw PGM file written by leptonica "
                    "(www.leptonica.com)\n%d %d\n%d\n", w, h, maxval);

        if (ds != 16) {
            for (i = 0; i < h; i++) {
                lines = datas + i * wpls;
                for (j = 0; j < w; j++) {
                    if (ds == 2)
                        val8 = GET_DATA_DIBIT(lines, j);
                    else if (ds == 4)
                        val8 = GET_DATA_QBIT(lines, j);
                    else  /* ds == 8 */
                        val8 = GET_DATA_BYTE(lines, j);
                    fwrite(&val8, 1, 1, fp);
                }
            }
        } else {  /* ds == 16 */
            for (i = 0; i < h; i++) {
                lines = datas + i * wpls;
                for (j = 0; j < w; j++) {
                    val16 = GET_DATA_TWO_BYTES(lines, j);
                    fwrite(&val16, 2, 1, fp);
                }
            }
        }
    } else {  /* rgb color */
        fprintf(fp, "P6\n# Raw PPM file written by leptonica "
                    "(www.leptonica.com)\n%d %d\n255\n", w, h);

        if (d == 24) {   /* packed, 3 bytes to a pixel */
            filebpl = 3 * w;
            for (i = 0; i < h; i++) {  /* write out each raster line */
                lines = datas + i * wpls;
                if (fwrite(lines, 1, filebpl, fp) != filebpl)
                    writeerror = 1;
            }
        } else {  /* 32 bpp rgb */
            for (i = 0; i < h; i++) {
                lines = datas + i * wpls;
                for (j = 0; j < wpls; j++) {
                    pword = lines + j;
                    pel[0] = GET_DATA_BYTE(pword, COLOR_RED);
                    pel[1] = GET_DATA_BYTE(pword, COLOR_GREEN);
                    pel[2] = GET_DATA_BYTE(pword, COLOR_BLUE);
                    if (fwrite(pel, 1, 3, fp) != 3)
                        writeerror = 1;
                }
            }
        }
    }

    pixDestroy(&pixs);
    if (writeerror)
        return ERROR_INT("image write fail", __func__, 1);
    return 0;
}


/*!
 * \brief   pixWriteStreamAsciiPnm()
 *
 * \param[in]   fp    file stream opened for write
 * \param[in]   pix
 * \return  0 if OK; 1 on error
 *
 *  Writes "ASCII" format only:
 *      1 bpp --> pbm P1
 *      2, 4, 8, 16 bpp, no colormap or grayscale colormap --> pgm P2
 *      2, 4, 8 bpp with color-valued colormap, or rgb --> rgb ppm P3
 */
l_ok
pixWriteStreamAsciiPnm(FILE  *fp,
                       PIX   *pix)
{
char       buffer[256];
l_uint8    cval[3];
l_int32    h, w, d, ds, i, j, k, maxval, count;
l_uint32   val;
PIX       *pixs;

    if (!fp)
        return ERROR_INT("fp not defined", __func__, 1);
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    pixGetDimensions(pix, &w, &h, &d);
    if (d != 1 && d != 2 && d != 4 && d != 8 && d != 16 && d != 32)
        return ERROR_INT("d not in {1,2,4,8,16,32}", __func__, 1);

        /* If a colormap exists, remove and convert to grayscale or rgb */
    if (pixGetColormap(pix) != NULL)
        pixs = pixRemoveColormap(pix, REMOVE_CMAP_BASED_ON_SRC);
    else
        pixs = pixClone(pix);
    ds =  pixGetDepth(pixs);

    if (ds == 1) {  /* binary */
        fprintf(fp, "P1\n# Ascii PBM file written by leptonica "
                    "(www.leptonica.com)\n%d %d\n", w, h);

        count = 0;
        for (i = 0; i < h; i++) {
            for (j = 0; j < w; j++) {
                pixGetPixel(pixs, j, i, &val);
                if (val == 0)
                    fputc('0', fp);
                else  /* val == 1 */
                    fputc('1', fp);
                fputc(' ', fp);
                count += 2;
                if (count >= 70) {
                    fputc('\n', fp);
                    count = 0;
                }
            }
        }
    } else if (ds == 2 || ds == 4 || ds == 8 || ds == 16) {  /* grayscale */
        maxval = (1 << ds) - 1;
        fprintf(fp, "P2\n# Ascii PGM file written by leptonica "
                    "(www.leptonica.com)\n%d %d\n%d\n", w, h, maxval);

        count = 0;
        for (i = 0; i < h; i++) {
            for (j = 0; j < w; j++) {
                pixGetPixel(pixs, j, i, &val);
                if (ds == 2) {
                    snprintf(buffer, sizeof(buffer), "%1d ", val);
                    fwrite(buffer, 1, 2, fp);
                    count += 2;
                } else if (ds == 4) {
                    snprintf(buffer, sizeof(buffer), "%2d ", val);
                    fwrite(buffer, 1, 3, fp);
                    count += 3;
                } else if (ds == 8) {
                    snprintf(buffer, sizeof(buffer), "%3d ", val);
                    fwrite(buffer, 1, 4, fp);
                    count += 4;
                } else {  /* ds == 16 */
                    snprintf(buffer, sizeof(buffer), "%5d ", val);
                    fwrite(buffer, 1, 6, fp);
                    count += 6;
                }
                if (count >= 60) {
                    fputc('\n', fp);
                    count = 0;
                }
            }
        }
    } else {  /* rgb color */
        fprintf(fp, "P3\n# Ascii PPM file written by leptonica "
                    "(www.leptonica.com)\n%d %d\n255\n", w, h);
        count = 0;
        for (i = 0; i < h; i++) {
            for (j = 0; j < w; j++) {
                pixGetPixel(pixs, j, i, &val);
                cval[0] = GET_DATA_BYTE(&val, COLOR_RED);
                cval[1] = GET_DATA_BYTE(&val, COLOR_GREEN);
                cval[2] = GET_DATA_BYTE(&val, COLOR_BLUE);
                for (k = 0; k < 3; k++) {
                    snprintf(buffer, sizeof(buffer), "%3d ", cval[k]);
                    fwrite(buffer, 1, 4, fp);
                    count += 4;
                    if (count >= 60) {
                        fputc('\n', fp);
                        count = 0;
                    }
                }
            }
        }
    }

    pixDestroy(&pixs);
    return 0;
}


/*!
 * \brief   pixWriteStreamPam()
 *
 * \param[in]   fp    file stream opened for write
 * \param[in]   pix
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This writes arbitrary PAM (P7) packed format.
 *      (2) 24 bpp rgb are not supported in leptonica, but this will
 *          write them out as a packed array of bytes (3 to a pixel).
 * </pre>
 */
l_ok
pixWriteStreamPam(FILE  *fp,
                  PIX   *pix)
{
l_uint8    val8;
l_uint8    pel[8];
l_uint16   val16;
l_int32    h, w, d, ds, i, j;
l_int32    wpls, spps, filebpl, writeerror, maxval;
l_uint32  *pword, *datas, *lines;
PIX       *pixs;

    if (!fp)
        return ERROR_INT("fp not defined", __func__, 1);
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    pixGetDimensions(pix, &w, &h, &d);
    if (d != 1 && d != 2 && d != 4 && d != 8 && d != 16 && d != 24 && d != 32)
        return ERROR_INT("d not in {1,2,4,8,16,24,32}", __func__, 1);

        /* If a colormap exists, remove and convert to grayscale or rgb */
    if (pixGetColormap(pix) != NULL)
        pixs = pixRemoveColormap(pix, REMOVE_CMAP_BASED_ON_SRC);
    else
        pixs = pixClone(pix);
    ds = pixGetDepth(pixs);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    spps = pixGetSpp(pixs);
    if (ds < 24)
        maxval = (1 << ds) - 1;
    else
        maxval = 255;

    writeerror = 0;
    fprintf(fp, "P7\n# Arbitrary PAM file written by leptonica "
                "(www.leptonica.com)\n");
    fprintf(fp, "WIDTH %d\n", w);
    fprintf(fp, "HEIGHT %d\n", h);
    fprintf(fp, "DEPTH %d\n", spps);
    fprintf(fp, "MAXVAL %d\n", maxval);
    if (spps == 1 && ds == 1)
        fprintf(fp, "TUPLTYPE BLACKANDWHITE\n");
    else if (spps == 1)
        fprintf(fp, "TUPLTYPE GRAYSCALE\n");
    else if (spps == 3)
        fprintf(fp, "TUPLTYPE RGB\n");
    else if (spps == 4)
        fprintf(fp, "TUPLTYPE RGB_ALPHA\n");
    fprintf(fp, "ENDHDR\n");

    switch (d) {
    case 1:
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            for (j = 0; j < w; j++) {
                val8 = GET_DATA_BIT(lines, j);
                val8 ^= 1;  /* pam apparently uses white-is-1 photometry */
                if (fwrite(&val8, 1, 1, fp) != 1)
                    writeerror = 1;
            }
        }
        break;

    case 2:
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            for (j = 0; j < w; j++) {
                val8 = GET_DATA_DIBIT(lines, j);
                if (fwrite(&val8, 1, 1, fp) != 1)
                    writeerror = 1;
            }
        }
        break;

    case 4:
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            for (j = 0; j < w; j++) {
                val8 = GET_DATA_QBIT(lines, j);
                if (fwrite(&val8, 1, 1, fp) != 1)
                    writeerror = 1;
            }
        }
        break;

    case 8:
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            for (j = 0; j < w; j++) {
                val8 = GET_DATA_BYTE(lines, j);
                if (fwrite(&val8, 1, 1, fp) != 1)
                    writeerror = 1;
            }
        }
        break;

    case 16:
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            for (j = 0; j < w; j++) {
                val16 = GET_DATA_TWO_BYTES(lines, j);
                if (fwrite(&val16, 2, 1, fp) != 1)
                    writeerror = 1;
            }
        }
        break;

    case 24:
        filebpl = 3 * w;
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            if (fwrite(lines, 1, filebpl, fp) != filebpl)
                writeerror = 1;
        }
        break;

    case 32:
        switch (spps) {
        case 3:
            for (i = 0; i < h; i++) {
                lines = datas + i * wpls;
                for (j = 0; j < wpls; j++) {
                    pword = lines + j;
                    pel[0] = GET_DATA_BYTE(pword, COLOR_RED);
                    pel[1] = GET_DATA_BYTE(pword, COLOR_GREEN);
                    pel[2] = GET_DATA_BYTE(pword, COLOR_BLUE);
                    if (fwrite(pel, 1, 3, fp) != 3)
                        writeerror = 1;
                }
            }
            break;
        case 4:
            for (i = 0; i < h; i++) {
                lines = datas + i * wpls;
                for (j = 0; j < wpls; j++) {
                    pword = lines + j;
                    pel[0] = GET_DATA_BYTE(pword, COLOR_RED);
                    pel[1] = GET_DATA_BYTE(pword, COLOR_GREEN);
                    pel[2] = GET_DATA_BYTE(pword, COLOR_BLUE);
                    pel[3] = GET_DATA_BYTE(pword, L_ALPHA_CHANNEL);
                    if (fwrite(pel, 1, 4, fp) != 4)
                        writeerror = 1;
                }
            }
            break;
        }
        break;
    }

    pixDestroy(&pixs);
    if (writeerror)
        return ERROR_INT("image write fail", __func__, 1);
    return 0;
}


/*---------------------------------------------------------------------*
 *                         Read/write to memory                        *
 *---------------------------------------------------------------------*/

/*!
 * \brief   pixReadMemPnm()
 *
 * \param[in]   data   const; pnm-encoded
 * \param[in]   size   of data
 * \return  pix, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The %size byte of %data must be a null character.
 * </pre>
 */
PIX *
pixReadMemPnm(const l_uint8  *data,
              size_t          size)
{
FILE  *fp;
PIX   *pix;

    if (!data)
        return (PIX *)ERROR_PTR("data not defined", __func__, NULL);
    if ((fp = fopenReadFromMemory(data, size)) == NULL)
        return (PIX *)ERROR_PTR("stream not opened", __func__, NULL);
    pix = pixReadStreamPnm(fp);
    fclose(fp);
    if (!pix) L_ERROR("pix not read\n", __func__);
    return pix;
}


/*!
 * \brief   readHeaderMemPnm()
 *
 * \param[in]    data    const; pnm-encoded
 * \param[in]    size    of data
 * \param[out]   pw      [optional]
 * \param[out]   ph      [optional]
 * \param[out]   pd      [optional]
 * \param[out]   ptype   [optional] pnm type
 * \param[out]   pbps    [optional] bits/sample
 * \param[out]   pspp    [optional] samples/pixel
 * \return  0 if OK, 1 on error
 */
l_ok
readHeaderMemPnm(const l_uint8  *data,
                 size_t          size,
                 l_int32        *pw,
                 l_int32        *ph,
                 l_int32        *pd,
                 l_int32        *ptype,
                 l_int32        *pbps,
                 l_int32        *pspp)
{
l_int32  ret;
FILE    *fp;

    if (!data)
        return ERROR_INT("data not defined", __func__, 1);

    if ((fp = fopenReadFromMemory(data, size)) == NULL)
        return ERROR_INT("stream not opened", __func__, 1);
    ret = freadHeaderPnm(fp, pw, ph, pd, ptype, pbps, pspp);
    fclose(fp);
    if (ret)
        return ERROR_INT("header data read failed", __func__, 1);
    return 0;
}


/*!
 * \brief   pixWriteMemPnm()
 *
 * \param[out]   pdata   data of PNM image
 * \param[out]   psize   size of returned data
 * \param[in]    pix
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) See pixWriteStreamPnm() for usage.  This version writes to
 *          memory instead of to a file stream.
 * </pre>
 */
l_ok
pixWriteMemPnm(l_uint8  **pdata,
               size_t    *psize,
               PIX       *pix)
{
l_int32  ret;
FILE    *fp;

    if (pdata) *pdata = NULL;
    if (psize) *psize = 0;
    if (!pdata)
        return ERROR_INT("&data not defined", __func__, 1 );
    if (!psize)
        return ERROR_INT("&size not defined", __func__, 1 );
    if (!pix)
        return ERROR_INT("&pix not defined", __func__, 1 );

#if HAVE_FMEMOPEN
    if ((fp = open_memstream((char **)pdata, psize)) == NULL)
        return ERROR_INT("stream not opened", __func__, 1);
    ret = pixWriteStreamPnm(fp, pix);
    fputc('\0', fp);
    fclose(fp);
    if (*psize > 0) *psize = *psize - 1;
#else
    L_INFO("no fmemopen API --> work-around: write to temp file\n", __func__);
  #ifdef _WIN32
    if ((fp = fopenWriteWinTempfile()) == NULL)
        return ERROR_INT("tmpfile stream not opened", __func__, 1);
  #else
    if ((fp = tmpfile()) == NULL)
        return ERROR_INT("tmpfile stream not opened", __func__, 1);
  #endif  /* _WIN32 */
    ret = pixWriteStreamPnm(fp, pix);
    rewind(fp);
    *pdata = l_binaryReadStream(fp, psize);
    fclose(fp);
#endif  /* HAVE_FMEMOPEN */
    return ret;
}


/*!
 * \brief   pixWriteMemPam()
 *
 * \param[out]   pdata   data of PAM image
 * \param[out]   psize   size of returned data
 * \param[in]    pix
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) See pixWriteStreamPnm() for usage.  This version writes to
 *          memory instead of to a file stream.
 * </pre>
 */
l_ok
pixWriteMemPam(l_uint8  **pdata,
               size_t    *psize,
               PIX       *pix)
{
l_int32  ret;
FILE    *fp;

    if (pdata) *pdata = NULL;
    if (psize) *psize = 0;
    if (!pdata)
        return ERROR_INT("&data not defined", __func__, 1 );
    if (!psize)
        return ERROR_INT("&size not defined", __func__, 1 );
    if (!pix)
        return ERROR_INT("&pix not defined", __func__, 1 );

#if HAVE_FMEMOPEN
    if ((fp = open_memstream((char **)pdata, psize)) == NULL)
        return ERROR_INT("stream not opened", __func__, 1);
    ret = pixWriteStreamPam(fp, pix);
    fputc('\0', fp);
    fclose(fp);
    if (*psize > 0) *psize = *psize - 1;
#else
    L_INFO("no fmemopen API --> work-around: write to temp file\n", __func__);
  #ifdef _WIN32
    if ((fp = fopenWriteWinTempfile()) == NULL)
        return ERROR_INT("tmpfile stream not opened", __func__, 1);
  #else
    if ((fp = tmpfile()) == NULL)
        return ERROR_INT("tmpfile stream not opened", __func__, 1);
  #endif  /* _WIN32 */
    ret = pixWriteStreamPam(fp, pix);
    rewind(fp);
    *pdata = l_binaryReadStream(fp, psize);
    fclose(fp);
#endif  /* HAVE_FMEMOPEN */
    return ret;
}


/*--------------------------------------------------------------------*
 *                          Static helpers                            *
 *--------------------------------------------------------------------*/
/*!
 * \brief   pnmReadNextAsciiValue()
 *
 *      Return: 0 if OK, 1 on error or EOF.
 *
 *  Notes:
 *      (1) This reads the next sample value in ASCII from the file.
 */
static l_int32
pnmReadNextAsciiValue(FILE     *fp,
                      l_int32  *pval)
{
l_int32  ignore;

    if (!pval)
        return ERROR_INT("&val not defined", __func__, 1);
    *pval = 0;
    if (!fp)
        return ERROR_INT("stream not open", __func__, 1);

    if (EOF == fscanf(fp, " "))
        return 1;
    if (1 != fscanf(fp, "%d", pval))
        return 1;

    return 0;
}


/*!
 * \brief   pnmReadNextNumber()
 *
 * \param[in]    fp    file stream
 * \param[out]   pval  value as an integer
 * \return  0 if OK, 1 on error or EOF.
 *
 * <pre>
 * Notes:
 *      (1) This reads the next set of numeric chars, returning
 *          the value and swallowing initial whitespaces and ONE
 *          trailing whitespace character.  This is needed to read
 *          the maxval in the header, which precedes the binary data.
 * </pre>
 */
static l_int32
pnmReadNextNumber(FILE     *fp,
                  l_int32  *pval)
{
char      buf[8];
l_int32   i, c, foundws;

    if (!pval)
        return ERROR_INT("&val not defined", __func__, 1);
    *pval = 0;
    if (!fp)
        return ERROR_INT("stream not open", __func__, 1);

        /* Swallow whitespace */
    if (fscanf(fp, " ") == EOF)
        return ERROR_INT("end of file reached", __func__, 1);

        /* The ASCII characters for the number are followed by exactly
         * one whitespace character. */
    foundws = FALSE;
    for (i = 0; i < 8; i++)
        buf[i] = '\0';
    for (i = 0; i < 8; i++) {
        if ((c = fgetc(fp)) == EOF)
            return ERROR_INT("end of file reached", __func__, 1);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            foundws = TRUE;
            buf[i] = '\n';
            break;
        }
        if (!isdigit(c))
            return ERROR_INT("char read is not a digit", __func__, 1);
        buf[i] = c;
    }
    if (!foundws)
        return ERROR_INT("no whitespace found", __func__, 1);
    if (sscanf(buf, "%d", pval) != 1)
        return ERROR_INT("invalid read", __func__, 1);
    return 0;
}

/*!
 * \brief   pnmReadNextString()
 *
 * \param[in]    fp    file stream
 * \param[out]   buff  pointer to the string buffer
 * \param[in]    size  max. number of characters in buffer
 * \return  0 if OK, 1 on error or EOF.
 *
 * <pre>
 * Notes:
 *      (1) This reads the next set of alphanumeric chars, returning the string.
 *          This is needed to read header lines, which precede the P7
 *          format binary data.
 * </pre>
 */
static l_int32
pnmReadNextString(FILE    *fp,
                  char    *buff,
                  l_int32  size)
{
char  fmtString[7];  /* must contain "%9999s" [*] */

    if (!buff)
        return ERROR_INT("buff not defined", __func__, 1);
    *buff = '\0';
    if (size > 10000)  /* size - 1 has > 4 digits [*]  */
        return ERROR_INT("size is too big", __func__, 1);
    if (size <= 0)
        return ERROR_INT("size is too small", __func__, 1);
    if (!fp)
        return ERROR_INT("stream not open", __func__, 1);

        /* Skip whitespace */
    if (fscanf(fp, " ") == EOF)
        return 1;

        /* Comment lines are allowed to appear anywhere in the header lines */
    if (pnmSkipCommentLines(fp))
        return ERROR_INT("end of file reached", __func__, 1);

    snprintf(fmtString, 7, "%%%ds", size - 1);
    if (fscanf(fp, fmtString, buff) == EOF)
        return 1;

    return 0;
}


/*!
 * \brief   pnmSkipCommentLines()
 *
 *      Return: 0 if OK, 1 on error or EOF
 *
 *  Notes:
 *      (1) Comment lines begin with '#'
 *      (2) Usage: caller should check return value for EOF
 *      (3) The previous implementation used fseek(fp, -1L, SEEK_CUR)
 *          to back up one character, which doesn't work with stdin.
 */
static l_int32
pnmSkipCommentLines(FILE  *fp)
{
l_int32  i;
char     c;

    if (!fp)
        return ERROR_INT("stream not open", __func__, 1);
    while ((i = fscanf(fp, "#%c", &c))) {
        if (i == EOF) return 1;
        while (c != '\n') {
            if (fscanf(fp, "%c", &c) == EOF)
                return 1;
        }
    }
    return 0;
}


/* --------------------------------------------*/
#endif  /* USE_PNMIO */
/* --------------------------------------------*/
