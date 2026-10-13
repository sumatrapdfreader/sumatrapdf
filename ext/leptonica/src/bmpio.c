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
 * \file bmpio.c
 * <pre>
 *
 *      Read bmp
 *           PIX          *pixReadStreamBmp()
 *           PIX          *pixReadMemBmp()
 *
 *      Write bmp
 *           l_int32       pixWriteStreamBmp()
 *           l_int32       pixWriteMemBmp()
 *
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"
#include "pix_internal.h"
#include "bmp.h"

/* --------------------------------------------*/
#if  USE_BMPIO   /* defined in environ.h */
/* --------------------------------------------*/

    /* Here we're setting the pixel value 0 to white (255) and the
     * value 1 to black (0).  This is the convention for grayscale, but
     * the opposite of the convention for 1 bpp, where 0 is white
     * and 1 is black.  Both colormap entries are opaque (alpha = 255) */
RGBA_QUAD   bwmap[2] = { {255,255,255,255}, {0,0,0,255} };

    /* Image dimension limits */
static const l_int32  L_MAX_ALLOWED_WIDTH = 1000000;
static const l_int32  L_MAX_ALLOWED_HEIGHT = 1000000;
static const l_int64  L_MAX_ALLOWED_PIXELS = 400000000LL;
static const l_int32  L_MAX_ALLOWED_RES = 10000000;  /* pixels/meter */

#ifndef  NO_CONSOLE_IO
#define  DEBUG     0
#endif  /* ~NO_CONSOLE_IO */

/*--------------------------------------------------------------*
 *                              Read bmp                        *
 *--------------------------------------------------------------*/
/*!
 * \brief   pixReadStreamBmp()
 *
 * \param[in]    fp file stream opened for read
 * \return  pix, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Here are references on the bmp file format:
 *          http://en.wikipedia.org/wiki/BMP_file_format
 *          http://www.fortunecity.com/skyscraper/windows/364/bmpffrmt.html
 * </pre>
 */
PIX *
pixReadStreamBmp(FILE  *fp)
{
l_uint8  *data;
size_t    size;
PIX      *pix;

    if (!fp)
        return (PIX *)ERROR_PTR("fp not defined", __func__, NULL);

        /* Read data from file and decode into Y,U,V arrays */
    rewind(fp);
    if ((data = l_binaryReadStream(fp, &size)) == NULL)
        return (PIX *)ERROR_PTR("data not read", __func__, NULL);

    pix = pixReadMemBmp(data, size);
    LEPT_FREE(data);
    return pix;
}


/*!
 * \brief   pixReadMemBmp()
 *
 * \param[in]    cdata    bmp data
 * \param[in]    size     number of bytes of bmp-formatted data
 * \return  pix, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The BMP file is organized as follows:
 *          * 14 byte fileheader
 *          * Variable size infoheader: 40, 108 or 124 bytes.
 *            We only use data in he first 40 bytes.
 *          * Optional colormap, with size 4 * ncolors (in bytes)
 *          * Image data
 *      (2) 2 bpp bmp files are not valid in the original spec, but they
 *          are valid in later versions.
 *      (3) We support reading rgb files with bpp = 24 and rgba files
 *          with 32 bpp.  For the latter, the transparency component of
 *          the generated pix is saved; however, for rgba images with
 *          non-opaque transparent components, png provides more flexibility.
 * </pre>
 */
PIX *
pixReadMemBmp(const l_uint8  *cdata,
              size_t          size)
{
l_uint8    pel[4];
l_uint8   *cmapBuf, *fdata, *data, *bmpih_b;
l_int16    bftype, depth, d;
l_int32    offset, width, height, height_neg, xres, yres, spp;
l_int32    compression, imagebytes, fdatabytes, cmapbytes, ncolors, maxcolors;
l_int32    fdatabpl, extrabytes, filebpp, pixWpl, pixBpl, i, j, k;
l_uint32   ihbytes;
l_uint32  *line, *pixdata, *pword;
l_int64    npixels;
BMP_FH    *bmpfh;
BMP_IH    bmpih;
PIX       *pix, *pix1;
PIXCMAP   *cmap;

    if (!cdata)
        return (PIX *)ERROR_PTR("cdata not defined", __func__, NULL);
    if (size < sizeof(BMP_FH) + sizeof(BMP_IH))
        return (PIX *)ERROR_PTR("bmf size error", __func__, NULL);

        /* Verify this is an uncompressed bmp */
    bmpfh = (BMP_FH *)cdata;
    bftype = bmpfh->bfType[0] + ((l_int32)bmpfh->bfType[1] << 8);
    if (bftype != BMP_ID)
        return (PIX *)ERROR_PTR("not bmf format", __func__, NULL);
    memcpy(&bmpih, cdata + BMP_FHBYTES, BMP_IHBYTES);
    compression = convertOnBigEnd32(bmpih.biCompression);
    if (compression != 0)
        return (PIX *)ERROR_PTR("cannot read compressed BMP files",
                                __func__, NULL);

        /* Find the offset from the beginning of the file to the image data */
    offset = bmpfh->bfOffBits[0];
    offset += (l_int32)bmpfh->bfOffBits[1] << 8;
    offset += (l_int32)bmpfh->bfOffBits[2] << 16;
    offset += (l_uint32)bmpfh->bfOffBits[3] << 24;

        /* Read the remaining useful data in the infoheader.
         * Note that the first 4 bytes give the infoheader size.
         * The infoheader pointer on sparc64 is not 32-bit aligned. */
    bmpih_b = (l_uint8 *)&bmpih;
    ihbytes = bmpih_b[0] | ((l_int32)bmpih_b[1] << 8) |
              ((l_int32)bmpih_b[2] << 16) | ((l_uint32)bmpih_b[3] << 24);
    width = convertOnBigEnd32(bmpih.biWidth);
    height = convertOnBigEnd32(bmpih.biHeight);
    depth = convertOnBigEnd16(bmpih.biBitCount);
    imagebytes = convertOnBigEnd32(bmpih.biSizeImage);
    xres = convertOnBigEnd32(bmpih.biXPelsPerMeter);
    yres = convertOnBigEnd32(bmpih.biYPelsPerMeter);

        /* Some sanity checking.  We impose limits on the image
         * dimensions, resolution and number of pixels.  We make sure the
         * file is the correct size to hold the amount of uncompressed data
         * that is specified in the header.  The number of colormap
         * entries is checked: it can be either 0 (no cmap) or some
         * number between 2 and 256.
         * Note that the imagebytes for uncompressed images is either
         * 0 or the size of the file data.  (The fact that it can
         * be 0 is perhaps some legacy glitch). */
    if (width < 1)
        return (PIX *)ERROR_PTR("width < 1", __func__, NULL);
    if (width > L_MAX_ALLOWED_WIDTH)
        return (PIX *)ERROR_PTR("width too large", __func__, NULL);
    if (height == 0 || height < -L_MAX_ALLOWED_HEIGHT ||
        height > L_MAX_ALLOWED_HEIGHT)
        return (PIX *)ERROR_PTR("invalid height", __func__, NULL);
    if (xres < 0 || xres > L_MAX_ALLOWED_RES ||
        yres < 0 || yres > L_MAX_ALLOWED_RES)
        return (PIX *)ERROR_PTR("invalid resolution", __func__, NULL);
    height_neg = 0;
    if (height < 0) {
        height_neg = 1;
        height = -height;
    }
    if (ihbytes != 40 && ihbytes != 108 && ihbytes != 124) {
        L_ERROR("invalid ihbytes = %d; not in {40, 108, 124}\n",
                __func__, ihbytes);
        return NULL;
    }
    npixels = 1LL * width * height;
    if (npixels > L_MAX_ALLOWED_PIXELS)
        return (PIX *)ERROR_PTR("npixels too large", __func__, NULL);
    if (depth != 1 && depth != 2 && depth != 4 && depth != 8 &&
        depth != 16 && depth != 24 && depth != 32) {
        L_ERROR("invalid depth = %d; not in {1, 2, 4, 8, 16, 24, 32}\n",
                __func__, depth);
        return NULL;
    }
    fdatabpl = 4 * ((1LL * width * depth + 31)/32);
    fdatabytes = fdatabpl * height;
    if (imagebytes != 0 && imagebytes != fdatabytes) {
        L_ERROR("invalid imagebytes = %d; not equal to fdatabytes = %d\n",
                __func__, imagebytes, fdatabytes);
        return NULL;
    }

        /* In the original spec, BITMAPINFOHEADER is 40 bytes.
         * There have been a number of revisions, to capture more information.
         * For example, the fifth version, BITMAPV5HEADER, adds 84 bytes
         * of ICC color profiles.  We use the size of the infoheader
         * to accommodate these newer formats.  Knowing the size of the
         * infoheader gives more opportunity to sanity check input params. */
    cmapbytes = offset - BMP_FHBYTES - ihbytes;
    ncolors = cmapbytes / sizeof(RGBA_QUAD);
    if (ncolors < 0 || ncolors == 1)
        return (PIX *)ERROR_PTR("invalid: cmap size < 0 or 1", __func__, NULL);
    if (ncolors > 0 && depth > 8)
        return (PIX *)ERROR_PTR("can't have cmap for d > 8", __func__, NULL);
    maxcolors = (depth <= 8) ? 1 << depth : 0;
    if (ncolors > maxcolors) {
        L_ERROR("cmap too large for depth %d: ncolors = %d > maxcolors = %d\n",
                __func__, depth, ncolors, maxcolors);
        return NULL;
    }
    if (size != 1LL * offset + 1LL * fdatabytes)
        return (PIX *)ERROR_PTR("size incommensurate with image data",
                                __func__,NULL);

        /* Handle the colormap */
    cmapBuf = NULL;
    if (ncolors > 0) {
        if ((cmapBuf = (l_uint8 *)LEPT_CALLOC(ncolors, sizeof(RGBA_QUAD)))
                 == NULL)
            return (PIX *)ERROR_PTR("cmapBuf alloc fail", __func__, NULL );

            /* Read the colormap entry data from bmp. The RGBA_QUAD colormap
             * entries are used for both bmp and leptonica colormaps. */
        memcpy(cmapBuf, cdata + BMP_FHBYTES + ihbytes,
               ncolors * sizeof(RGBA_QUAD));
    }

        /* Make a 32 bpp pix if file depth is 24 bpp */
    d = (depth == 24) ? 32 : depth;
    if ((pix = pixCreate(width, height, d)) == NULL) {
        LEPT_FREE(cmapBuf);
        return (PIX *)ERROR_PTR( "pix not made", __func__, NULL);
    }
    pixSetXRes(pix, (l_int32)((l_float32)xres / 39.37 + 0.5));  /* to ppi */
    pixSetYRes(pix, (l_int32)((l_float32)yres / 39.37 + 0.5));  /* to ppi */
    pixSetInputFormat(pix, IFF_BMP);
    pixWpl = pixGetWpl(pix);
    pixBpl = 4 * pixWpl;
    if (depth <= 16)
        spp = 1;
    else if (depth == 24)
        spp = 3;
    else  /* depth == 32 */
        spp = 4;
    pixSetSpp(pix, spp);

        /* Convert the bmp colormap to a pixcmap */
    cmap = NULL;
    if (ncolors > 0) {  /* import the colormap to the pix cmap */
        cmap = pixcmapCreate(L_MIN(d, 8));
        LEPT_FREE(cmap->array);  /* remove generated cmap array */
        cmap->array  = (void *)cmapBuf;  /* and replace */
        cmap->n = L_MIN(ncolors, 256);
        for (i = 0; i < cmap->n; i++)   /* set all colors opaque */
            pixcmapSetAlpha (cmap, i, 255);
    }
    if (pixSetColormap(pix, cmap)) {
        pixDestroy(&pix);
        return (PIX *)ERROR_PTR("invalid colormap", __func__, NULL);
    }

        /* Acquire the image data.  Image origin for bmp is at lower right. */
    fdata = (l_uint8 *)cdata + offset;  /* start of the bmp image data */
    pixdata = pixGetData(pix);
    if (depth != 24 && depth != 32) {  /* typ. 1 or 8 bpp */
        data = (l_uint8 *)pixdata + pixBpl * (height - 1);
        for (i = 0; i < height; i++) {
            memcpy(data, fdata, fdatabpl);
            fdata += fdatabpl;
            data -= pixBpl;
        }
    } else {  /*  24 or 32 bpp file; 32 bpp pix
             *  Note: for rgb bmp files, pel[0] is blue, pel[1] is green,
             *  and pel[2] is red.  This is opposite to the storage
             *  in the pix, which puts the red pixel in the 0 byte,
             *  the green in the 1 byte and the blue in the 2 byte.
             *  Note also that all words are endian flipped after
             *  assignment on L_LITTLE_ENDIAN platforms.
             *
             *  We can then make these assignments for little endians:
             *      SET_DATA_BYTE(pword, 1, pel[0]);      blue
             *      SET_DATA_BYTE(pword, 2, pel[1]);      green
             *      SET_DATA_BYTE(pword, 3, pel[2]);      red
             *  This looks like:
             *          3  (R)     2  (G)        1  (B)        0
             *      |-----------|------------|-----------|-----------|
             *  and after byte flipping:
             *           3          2  (B)     1  (G)        0  (R)
             *      |-----------|------------|-----------|-----------|
             *
             *  For big endians we set:
             *      SET_DATA_BYTE(pword, 2, pel[0]);      blue
             *      SET_DATA_BYTE(pword, 1, pel[1]);      green
             *      SET_DATA_BYTE(pword, 0, pel[2]);      red
             *  This looks like:
             *          0  (R)     1  (G)        2  (B)        3
             *      |-----------|------------|-----------|-----------|
             *  so in both cases we get the correct assignment in the PIX.
             *
             *  Can we do a platform-independent assignment?
             *  Yes, set the bytes without using macros:
             *      *((l_uint8 *)pword) = pel[2];           red
             *      *((l_uint8 *)pword + 1) = pel[1];       green
             *      *((l_uint8 *)pword + 2) = pel[0];       blue
             *  For little endians, before flipping, this looks again like:
             *          3  (R)     2  (G)        1  (B)        0
             *      |-----------|------------|-----------|-----------|
             *
             *  For reading an spp == 4 file with a transparency component,
             *  the code below shows where the alpha component is located
             *  in each pixel.
             */
        filebpp = (depth == 24) ? 3 : 4;
        extrabytes = fdatabpl - filebpp * width;
        line = pixdata + pixWpl * (height - 1);
        for (i = 0; i < height; i++) {
            for (j = 0; j < width; j++) {
                pword = line + j;
                memcpy(&pel, fdata, filebpp);
                fdata += filebpp;
                *((l_uint8 *)pword + COLOR_RED) = pel[2];
                *((l_uint8 *)pword + COLOR_GREEN) = pel[1];
                *((l_uint8 *)pword + COLOR_BLUE) = pel[0];
                    /* Set the alpha byte to opaque for rgb */
                if (depth == 24)
                    *((l_uint8 *)pword + L_ALPHA_CHANNEL) = 255;
                else
                    *((l_uint8 *)pword + L_ALPHA_CHANNEL) = pel[3];
            }
            if (extrabytes) {
                for (k = 0; k < extrabytes; k++) {
                    memcpy(&pel, fdata, 1);
                    fdata++;
                }
            }
            line -= pixWpl;
        }
    }

    pixEndianByteSwap(pix);
    if (height_neg)
        pixFlipTB(pix, pix);

        /* ----------------------------------------------
         * We do not use 1 bpp pix with colormaps in leptonica.
         * The colormap must be removed in such a way that the pixel
         * values are not changed.  If the values are only black and
         * white, return a 1 bpp image; if gray, return an 8 bpp pix;
         * otherwise, return a 32 bpp rgb pix.
         * ---------------------------------------------- */
    if (depth == 1 && cmap) {
        L_INFO("removing opaque cmap from 1 bpp\n", __func__);
        pix1 = pixRemoveColormap(pix, REMOVE_CMAP_BASED_ON_SRC);
        pixDestroy(&pix);
        pix = pix1;  /* rename */
    }

    return pix;
}


/*--------------------------------------------------------------*
 *                            Write bmp                         *
 *--------------------------------------------------------------*/
/*!
 * \brief   pixWriteStreamBmp()
 *
 * \param[in]    fp     file stream
 * \param[in]    pix    all depths
 * \return  0 if OK, 1 on error
 */
l_ok
pixWriteStreamBmp(FILE  *fp,
                  PIX   *pix)
{
l_uint8  *data;
size_t    size, nbytes;

    if (!fp)
        return ERROR_INT("stream not defined", __func__, 1);
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    pixWriteMemBmp(&data, &size, pix);
    rewind(fp);
    nbytes = fwrite(data, 1, size, fp);
    LEPT_FREE(data);
    if (nbytes != size) {
        L_ERROR("Truncation: nbytes = %zu, size = %zu\n",
                __func__,  nbytes, size);
        return ERROR_INT("Write error", __func__, 1);
    }
    return 0;
}


/*!
 * \brief   pixWriteMemBmp()
 *
 * \param[out]   pfdata   data of bmp formatted image
 * \param[out]   pfsize    size of returned data
 * \param[in]    pixs      1, 2, 4, 8, 16, 32 bpp
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) 2 bpp bmp files are not valid in the original spec, and are
 *          written as 8 bpp.
 *      (2) pix with depth <= 8 bpp are written with a colormap.
 *          16 bpp gray and 32 bpp rgb pix are written without a colormap.
 *      (3) The transparency component in an rgba (spp = 4) pix is written.
 *      (4) The bmp colormap entries, RGBA_QUAD, are the same as
 *          the ones used for colormaps in leptonica.  This allows
 *          a simple memcpy for bmp output.
 * </pre>
 */
l_ok
pixWriteMemBmp(l_uint8  **pfdata,
               size_t    *pfsize,
               PIX       *pixs)
{
l_uint8     pel[4];
l_uint8    *cta = NULL;     /* address of the bmp color table array */
l_uint8    *fdata, *data, *fmdata;
l_int32     cmaplen;      /* number of bytes in the bmp colormap */
l_int32     ncolors, val, stepsize, w, h, d, fdepth, xres, yres, valid;
l_int32     pixWpl, pixBpl, extrabytes, spp, fBpl, fWpl, i, j, k;
l_int32     heapcm;  /* extra copy of cta on the heap ? 1 : 0 */
l_uint32    offbytes, fimagebytes;
l_uint32   *line, *pword;
size_t      fsize;
BMP_FH     *bmpfh;
BMP_IH     bmpih;
PIX        *pix;
PIXCMAP    *cmap;
RGBA_QUAD  *pquad;

    if (pfdata) *pfdata = NULL;
    if (pfsize) *pfsize = 0;
    if (!pfdata)
        return ERROR_INT("&fdata not defined", __func__, 1 );
    if (!pfsize)
        return ERROR_INT("&fsize not defined", __func__, 1 );
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);

        /* Verify validity of colormap */
    if ((cmap = pixGetColormap(pixs)) != NULL) {
        pixcmapIsValid(cmap, pixs, &valid);
        if (!valid)
            return ERROR_INT("colormap is not valid", __func__, 1);
    }

    pixGetDimensions(pixs, &w, &h, &d);
    spp = pixGetSpp(pixs);
    if (spp != 1 && spp != 3 && spp != 4) {
        L_ERROR("unsupported spp = %d\n", __func__, spp);
        return 1;
    } 
    if (d == 2) {
        L_WARNING("2 bpp files can't be read; converting to 8 bpp\n",
                  __func__);
        pix = pixConvert2To8(pixs, 0, 85, 170, 255, 1);
        d = 8;
    } else if (d == 24) {
        pix = pixConvert24To32(pixs);
        d = 32;
    } else {
        pix = pixCopy(NULL, pixs);
    }

        /* Find the bits/pixel to be written to file */
    if (spp == 1)
        fdepth = d;
    else if (spp == 3)
        fdepth = 24;
    else  /* spp == 4 */
        fdepth = 32;

        /* Resolution is given in pixels/meter */
    xres = (l_int32)(39.37 * (l_float32)pixGetXRes(pix) + 0.5);
    yres = (l_int32)(39.37 * (l_float32)pixGetYRes(pix) + 0.5);

    pixWpl = pixGetWpl(pix);
    pixBpl = 4 * pixWpl;
    fWpl = (w * fdepth + 31) / 32;
    fBpl = 4 * fWpl;
    fimagebytes = h * fBpl;
    if (fimagebytes > 4LL * L_MAX_ALLOWED_PIXELS) {
        pixDestroy(&pix);
        return ERROR_INT("image data is too large", __func__, 1);
    }

        /* If not rgb or 16 bpp, the bmp data is required to have a colormap */
    heapcm = 0;
    if (d == 32 || d == 16) {   /* 24 bpp rgb or 16 bpp: no colormap */
        ncolors = 0;
        cmaplen = 0;
    } else if ((cmap = pixGetColormap(pix))) {   /* existing colormap */
        ncolors = pixcmapGetCount(cmap);
        cmaplen = ncolors * sizeof(RGBA_QUAD);
        cta = (l_uint8 *)cmap->array;
    } else {   /* no existing colormap; d <= 8; make a binary or gray one */
        if (d == 1) {
            cmaplen  = sizeof(bwmap);
            ncolors = 2;
            cta = (l_uint8 *)bwmap;
        } else {   /* d = 2,4,8; use a grayscale output colormap */
            ncolors = 1 << d;
            cmaplen = ncolors * sizeof(RGBA_QUAD);
            heapcm = 1;
            cta = (l_uint8 *)LEPT_CALLOC(cmaplen, 1);
            stepsize = 255 / (ncolors - 1);
            for (i = 0, val = 0, pquad = (RGBA_QUAD *)cta;
                 i < ncolors;
                 i++, val += stepsize, pquad++) {
                pquad->blue = pquad->green = pquad->red = val;
                pquad->alpha = 255;  /* opaque */
            }
        }
    }

#if DEBUG
    if (pixGetColormap(pix) != NULL) {
        l_uint8  *pcmptr;
        pcmptr = (l_uint8 *)pixGetColormap(pix)->array;
        lept_stderr("Pix colormap[0] = %c%c%c%d\n",
                    pcmptr[0], pcmptr[1], pcmptr[2], pcmptr[3]);
        lept_stderr("Pix colormap[1] = %c%c%c%d\n",
                    pcmptr[4], pcmptr[5], pcmptr[6], pcmptr[7]);
    }
#endif  /* DEBUG */

    offbytes = BMP_FHBYTES + BMP_IHBYTES + cmaplen;
    fsize = offbytes + fimagebytes;
    fdata = (l_uint8 *)LEPT_CALLOC(fsize, 1);
    *pfdata = fdata;
    *pfsize = fsize;

        /* Write little-endian file header data */
    bmpfh = (BMP_FH *)fdata;
    bmpfh->bfType[0] = (l_uint8)(BMP_ID >> 0);
    bmpfh->bfType[1] = (l_uint8)(BMP_ID >> 8);
    bmpfh->bfSize[0] = (l_uint8)(fsize >>  0);
    bmpfh->bfSize[1] = (l_uint8)(fsize >>  8);
    bmpfh->bfSize[2] = (l_uint8)(fsize >> 16);
    bmpfh->bfSize[3] = (l_uint8)(fsize >> 24);
    bmpfh->bfOffBits[0] = (l_uint8)(offbytes >>  0);
    bmpfh->bfOffBits[1] = (l_uint8)(offbytes >>  8);
    bmpfh->bfOffBits[2] = (l_uint8)(offbytes >> 16);
    bmpfh->bfOffBits[3] = (l_uint8)(offbytes >> 24);

        /* Convert to little-endian and write the info header data */
    bmpih.biSize = convertOnBigEnd32(BMP_IHBYTES);
    bmpih.biWidth = convertOnBigEnd32(w);
    bmpih.biHeight = convertOnBigEnd32(h);
    bmpih.biPlanes = convertOnBigEnd16(1);
    bmpih.biBitCount = convertOnBigEnd16(fdepth);
    bmpih.biCompression = 0;
    bmpih.biSizeImage = convertOnBigEnd32(fimagebytes);
    bmpih.biXPelsPerMeter = convertOnBigEnd32(xres);
    bmpih.biYPelsPerMeter = convertOnBigEnd32(yres);
    bmpih.biClrUsed = convertOnBigEnd32(ncolors);
    bmpih.biClrImportant = convertOnBigEnd32(ncolors);
    memcpy(fdata + BMP_FHBYTES, &bmpih, BMP_IHBYTES);

        /* Copy the colormap data and free the cta if necessary */
    if (ncolors > 0) {
        memcpy(fdata + BMP_FHBYTES + BMP_IHBYTES, cta, cmaplen);
        if (heapcm) LEPT_FREE(cta);
    }

        /* When you write a binary image with a colormap
         * that sets BLACK to 0, you must invert the data */
    if (fdepth == 1 && cmap && ((l_uint8 *)(cmap->array))[0] == 0x0) {
        pixInvert(pix, pix);
    }

        /* An endian byte swap is also required */
    pixEndianByteSwap(pix);

        /* Transfer the image data.  Image origin for bmp is at lower right. */
    fmdata = fdata + offbytes;
    if (fdepth != 24 && fdepth != 32) {   /* typ 1 or 8 bpp */
        data = (l_uint8 *)pixGetData(pix) + pixBpl * (h - 1);
        for (i = 0; i < h; i++) {
            memcpy(fmdata, data, fBpl);
            data -= pixBpl;
            fmdata += fBpl;
        }
    } else {  /* 32 bpp pix; 24 bpp or 32 bpp file
             *  See the comments in pixReadStreamBmp() to
             *  understand the logic behind the pixel ordering below.
             *  Note that we have again done an endian swap on
             *  little endian machines before arriving here, so that
             *  the bytes are ordered on both platforms as:
             *           Red        Green        Blue         --
             *      |-----------|------------|-----------|-----------|
             *
             *  For writing an spp == 4 file, the code below shows where
             *  the alpha component is written to file in each pixel.
             */
        extrabytes = fBpl - spp * w;
        line = pixGetData(pix) + pixWpl * (h - 1);
        for (i = 0; i < h; i++) {
            for (j = 0; j < w; j++) {
                pword = line + j;
                pel[2] = *((l_uint8 *)pword + COLOR_RED);
                pel[1] = *((l_uint8 *)pword + COLOR_GREEN);
                pel[0] = *((l_uint8 *)pword + COLOR_BLUE);
                if (spp == 4)
                    pel[3] = *((l_uint8 *)pword + L_ALPHA_CHANNEL);
                memcpy(fmdata, &pel, spp);
                fmdata += spp;
            }
            if (extrabytes) {
                for (k = 0; k < extrabytes; k++) {
                    memcpy(fmdata, &pel, 1);
                    fmdata++;
                }
            }
            line -= pixWpl;
        }
    }

    pixDestroy(&pix);
    return 0;
}

/* --------------------------------------------*/
#endif  /* USE_BMPIO */
