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
 * \file spixio.c
 * <pre>
 *
 *    This does fast serialization of a pix in memory to file,
 *    copying the raw data for maximum speed.  The underlying
 *    function serializes it to memory, and it is wrapped to be
 *    callable from standard pixRead() and pixWrite() file functions.
 *
 *      Reading spix from file
 *           PIX        *pixReadStreamSpix()
 *           l_int32     readHeaderSpix()
 *           l_int32     freadHeaderSpix()
 *           l_int32     sreadHeaderSpix()
 *
 *      Writing spix to file
 *           l_int32     pixWriteStreamSpix()
 *
 *      Low-level serialization of pix to/from memory (uncompressed)
 *           PIX        *pixReadMemSpix()
 *           l_int32     pixWriteMemSpix()
 *           l_int32     pixSerializeToMemory()
 *           PIX        *pixDeserializeFromMemory()
 *
 *    Note: these functions have not been extensively tested for fuzzing
 *    (bad input data that can result in, e.g., memory faults).
 *    The spix serialization format is only defined here, in leptonica.
 *    The image data is uncompressed and the serialization is not intended
 *    to be a secure file format from untrusted sources.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"

    /* Image dimension limits */
static const l_int32  MaxAllowedWidth = 1000000;
static const l_int32  MaxAllowedHeight = 1000000;
static const l_int64  MaxAllowedArea = 400000000LL;

#ifndef  NO_CONSOLE_IO
#define  DEBUG_SERIALIZE      0
#endif  /* ~NO_CONSOLE_IO */


/*-----------------------------------------------------------------------*
 *                          Reading spix from file                       *
 *-----------------------------------------------------------------------*/
/*!
 * \brief   pixReadStreamSpix()
 *
 * \param[in]    fp     file stream
 * \return  pix, or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) If called from pixReadStream(), the stream is positioned
 *          at the beginning of the file.
 * </pre>
 */
PIX *
pixReadStreamSpix(FILE  *fp)
{
size_t    nbytes;
l_uint8  *data;
PIX      *pix;

    if (!fp)
        return (PIX *)ERROR_PTR("stream not defined", __func__, NULL);

    if ((data = l_binaryReadStream(fp, &nbytes)) == NULL)
        return (PIX *)ERROR_PTR("data not read", __func__, NULL);
    pix = pixReadMemSpix(data, nbytes);
    LEPT_FREE(data);
    if (!pix)
        return (PIX *)ERROR_PTR("pix not made", __func__, NULL);
    return pix;
}


/*!
 * \brief   readHeaderSpix()
 *
 * \param[in]    filename
 * \param[out]   pwidth     width
 * \param[out]   pheight    height
 * \param[out]   pbps       bits/sample
 * \param[out]   pspp       samples/pixel
 * \param[out]   piscmap    [optional]  input NULL to ignore
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If there is a colormap, iscmap is returned as 1; else 0.
 * </pre>
 */
l_ok
readHeaderSpix(const char *filename,
               l_int32    *pwidth,
               l_int32    *pheight,
               l_int32    *pbps,
               l_int32    *pspp,
               l_int32    *piscmap)
{
l_int32  ret;
FILE    *fp;

    if (!filename)
        return ERROR_INT("filename not defined", __func__, 1);
    if (!pwidth || !pheight || !pbps || !pspp)
        return ERROR_INT("input ptr(s) not defined", __func__, 1);
    if ((fp = fopenReadStream(filename)) == NULL)
        return ERROR_INT_1("image file not found", filename, __func__, 1);
    ret = freadHeaderSpix(fp, pwidth, pheight, pbps, pspp, piscmap);
    fclose(fp);
    return ret;
}


/*!
 * \brief   freadHeaderSpix()
 *
 * \param[in]    fp        file stream
 * \param[out]   pwidth    width
 * \param[out]   pheight   height
 * \param[out]   pbps      bits/sample
 * \param[out]   pspp      samples/pixel
 * \param[out]   piscmap   [optional]  input NULL to ignore
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If there is a colormap, iscmap is returned as 1; else 0.
 * </pre>
 */
l_ok
freadHeaderSpix(FILE     *fp,
                l_int32  *pwidth,
                l_int32  *pheight,
                l_int32  *pbps,
                l_int32  *pspp,
                l_int32  *piscmap)
{
l_int32   nbytes, ret;
l_uint32  data[6];

    if (!fp)
        return ERROR_INT("stream not defined", __func__, 1);
    if (!pwidth || !pheight || !pbps || !pspp)
        return ERROR_INT("input ptr(s) not defined", __func__, 1);

    nbytes = fnbytesInFile(fp);
    if (nbytes < 32)
        return ERROR_INT("file too small to be spix", __func__, 1);
    if (fread(data, 4, 6, fp) != 6)
        return ERROR_INT("error reading data", __func__, 1);
    ret = sreadHeaderSpix(data, nbytes, pwidth, pheight, pbps, pspp, piscmap);
    return ret;
}


/*!
 * \brief   sreadHeaderSpix()
 *
 * \param[in]    data
 * \param[in]    size      of data
 * \param[out]   pwidth    width
 * \param[out]   pheight   height
 * \param[out]   pbps      bits/sample
 * \param[out]   pspp      samples/pixel
 * \param[out]   piscmap   [optional]  input NULL to ignore
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If there is a colormap, iscmap is returned as 1; else 0.
 * </pre>
 */
l_ok
sreadHeaderSpix(const l_uint32  *data,
                size_t           size,
                l_int32         *pwidth,
                l_int32         *pheight,
                l_int32         *pbps,
                l_int32         *pspp,
                l_int32         *piscmap)
{
char    *id;
l_int32  d, ncolors;

    if (!data)
        return ERROR_INT("data not defined", __func__, 1);
    if (!pwidth || !pheight || !pbps || !pspp)
        return ERROR_INT("input ptr(s) not defined", __func__, 1);
    *pwidth = *pheight = *pbps = *pspp = 0;
    if (piscmap)
      *piscmap = 0;
    if (size < 28)
        return ERROR_INT("size too small", __func__, 1);

        /* Check file id */
    id = (char *)data;
    if (id[0] != 's' || id[1] != 'p' || id[2] != 'i' || id[3] != 'x')
        return ERROR_INT("not a valid spix file", __func__, 1);

    *pwidth = data[1];
    *pheight = data[2];
    d = data[3];
    if (d <= 16) {
      *pbps = d;
      *pspp = 1;
    } else {
      *pbps = 8;
      *pspp = d / 8;  /* if the pix is 32 bpp, call it 4 samples */
    }
    ncolors = data[5];
    if (piscmap)
        *piscmap = (ncolors == 0) ? 0 : 1;

    return 0;
}


/*-----------------------------------------------------------------------*
 *                            Writing spix to file                       *
 *-----------------------------------------------------------------------*/
/*!
 * \brief   pixWriteStreamSpix()
 *
 * \param[in]    fp     file stream
 * \param[in]    pix
 * \return  0 if OK; 1 on error
 */
l_ok
pixWriteStreamSpix(FILE  *fp,
                   PIX   *pix)
{
l_uint8  *data;
size_t    size;

    if (!fp)
        return ERROR_INT("stream not defined", __func__, 1);
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    if (pixWriteMemSpix(&data, &size, pix))
        return ERROR_INT("failure to write pix to memory", __func__, 1);
    fwrite(data, 1, size, fp);
    LEPT_FREE(data);
    return 0;
}


/*-----------------------------------------------------------------------*
 *       Low-level serialization of pix to/from memory (uncompressed)    *
 *-----------------------------------------------------------------------*/
/*!
 * \brief   pixReadMemSpix()
 *
 * \param[in]    data    const; uncompressed
 * \param[in]    size    bytes of data
 * \return  pix, or NULL on error
 */
PIX *
pixReadMemSpix(const l_uint8  *data,
               size_t          size)
{
    return pixDeserializeFromMemory((l_uint32 *)data, size);
}


/*!
 * \brief   pixWriteMemSpix()
 *
 * \param[out]   pdata    data of serialized, uncompressed pix
 * \param[out]   psize    size of returned data
 * \param[in]    pix      all depths; colormap OK
 * \return  0 if OK, 1 on error
 */
l_ok
pixWriteMemSpix(l_uint8  **pdata,
                size_t    *psize,
                PIX       *pix)
{
    return pixSerializeToMemory(pix, (l_uint32 **)pdata, psize);
}


/*!
 * \brief   pixSerializeToMemory()
 *
 * \param[in]    pixs     all depths, colormap OK
 * \param[out]   pdata    serialized data in memory
 * \param[out]   pnbytes  number of bytes in data string
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This does a fast serialization of the principal elements
 *          of the pix, as follows:
 *            "spix"    (4 bytes) -- ID for file type
 *            w         (4 bytes)
 *            h         (4 bytes)
 *            d         (4 bytes)
 *            wpl       (4 bytes)
 *            ncolors   (4 bytes) -- in colormap; 0 if there is no colormap
 *            cdata     (4 * ncolors)  -- size of serialized colormap array
 *            rdatasize (4 bytes) -- size of serialized raster data
 *                                   = 4 * wpl * h
 *            rdata     (rdatasize)
 * </pre>
 */
l_ok
pixSerializeToMemory(PIX        *pixs,
                     l_uint32  **pdata,
                     size_t     *pnbytes)
{
char      *id;
l_int32    w, h, d, wpl, rdatasize, ncolors, nbytes, index, valid;
l_uint8   *cdata;  /* data in colormap array (4 bytes/color table entry) */
l_uint32  *data;
l_uint32  *rdata;  /* data in pix raster */
PIXCMAP   *cmap;

    if (!pdata || !pnbytes)
        return ERROR_INT("&data and &nbytes not both defined", __func__, 1);
    *pdata = NULL;
    *pnbytes = 0;
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);

    pixGetDimensions(pixs, &w, &h, &d);
    wpl = pixGetWpl(pixs);
    rdata = pixGetData(pixs);
    rdatasize = 4 * wpl * h;
    ncolors = 0;
    cdata = NULL;
    if ((cmap = pixGetColormap(pixs)) != NULL) {
        pixcmapIsValid(cmap, pixs, &valid);
        if (!valid)
            return ERROR_INT("colormap not valid", __func__, 1);
        pixcmapSerializeToMemory(cmap, 4, &ncolors, &cdata);
    }

    nbytes = 24 + 4 * ncolors + 4 + rdatasize;
    if ((data = (l_uint32 *)LEPT_CALLOC(nbytes / 4, sizeof(l_uint32)))
         == NULL) {
        LEPT_FREE(cdata);
        return ERROR_INT("data not made", __func__, 1);
    }
    *pdata = data;
    *pnbytes = nbytes;
    id = (char *)data;
    id[0] = 's';
    id[1] = 'p';
    id[2] = 'i';
    id[3] = 'x';
    data[1] = w;
    data[2] = h;
    data[3] = d;
    data[4] = wpl;
    data[5] = ncolors;
    if (ncolors > 0)
        memcpy(data + 6, cdata, 4 * ncolors);
    index = 6 + ncolors;
    data[index] = rdatasize;
    memcpy(data + index + 1, rdata, rdatasize);

#if  DEBUG_SERIALIZE
    lept_stderr("Serialize:   "
                "raster size = %d, ncolors in cmap = %d, total bytes = %d\n",
                rdatasize, ncolors, nbytes);
#endif  /* DEBUG_SERIALIZE */

    LEPT_FREE(cdata);
    return 0;
}


/*!
 * \brief   pixDeserializeFromMemory()
 *
 * \param[in]    data     serialized data in memory
 * \param[in]    nbytes   number of bytes in data string
 * \return  pix, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) See pixSerializeToMemory() for the binary format.
 *      (2) Note the image size limits.
 * </pre>
 */
PIX *
pixDeserializeFromMemory(const l_uint32  *data,
                         size_t           nbytes)
{
char      *id;
l_int32    w, h, d, pixdata_size, memdata_size, imdata_size, ncolors, valid;
l_uint32  *imdata;  /* data in pix raster */
PIX       *pix1, *pixd;
PIXCMAP   *cmap = NULL;

    if (!data)
        return (PIX *)ERROR_PTR("data not defined", __func__, NULL);
    if (nbytes < 28 || nbytes > ((1LL << 31) - 1)) {
        L_ERROR("invalid nbytes = %zu\n", __func__, nbytes);
        return NULL;
    }

    id = (char *)data;
    if (id[0] != 's' || id[1] != 'p' || id[2] != 'i' || id[3] != 'x')
        return (PIX *)ERROR_PTR("invalid id string", __func__, NULL);
    w = data[1];
    h = data[2];
    d = data[3];
    ncolors = data[5];

        /* Sanity checks on the amount of image data */
    if (w < 1 || w > MaxAllowedWidth)
        return (PIX *)ERROR_PTR("invalid width", __func__, NULL);
    if (h < 1 || h > MaxAllowedHeight)
        return (PIX *)ERROR_PTR("invalid height", __func__, NULL);
    if (1LL * w * h > MaxAllowedArea)
        return (PIX *)ERROR_PTR("area too large", __func__, NULL);
    if (ncolors < 0 || ncolors > 256 || ncolors + 7 >= nbytes/sizeof(l_int32))
        return (PIX *)ERROR_PTR("invalid ncolors", __func__, NULL);
    if ((pix1 = pixCreateHeader(w, h, d)) == NULL)  /* just make the header */
        return (PIX *)ERROR_PTR("failed to make header", __func__, NULL);
    pixdata_size = 4 * h * pixGetWpl(pix1);
    memdata_size = nbytes - 24 - 4 * ncolors - 4;
    imdata_size = data[6 + ncolors];
    pixDestroy(&pix1);
    if (pixdata_size != memdata_size || pixdata_size != imdata_size) {
        L_ERROR("pixdata_size = %d, memdata_size = %d, imdata_size = %d "
                "not all equal!\n", __func__, pixdata_size, memdata_size,
                imdata_size);
        return NULL;
    }

    if ((pixd = pixCreate(w, h, d)) == NULL)
        return (PIX *)ERROR_PTR("pix not made", __func__, NULL);
    if (ncolors > 0) {
        cmap = pixcmapDeserializeFromMemory((l_uint8 *)(&data[6]), 4, ncolors);
        if (!cmap) {
            pixDestroy(&pixd);
            return (PIX *)ERROR_PTR("cmap not made", __func__, NULL);
        }
        if (pixSetColormap(pixd, cmap)) {
            pixDestroy(&pixd);
            return (PIX *)ERROR_PTR("cmap is not valid", __func__, NULL);
        }
    }

        /* Read the raster data */
    imdata = pixGetData(pixd);
    memcpy(imdata, data + 7 + ncolors, imdata_size);

        /* Verify that the colormap is valid with the pix */
    if (ncolors > 0) {
        pixcmapIsValid(cmap, pixd, &valid);
        if (!valid) {
            pixDestroy(&pixd);
            return (PIX *)ERROR_PTR("cmap is invalid with pix", __func__, NULL);
        }
    }

#if  DEBUG_SERIALIZE
    lept_stderr("Deserialize: "
                "raster size = %d, ncolors in cmap = %d, total bytes = %zu\n",
                imdata_size, ncolors, nbytes);
#endif  /* DEBUG_SERIALIZE */

    return pixd;
}
