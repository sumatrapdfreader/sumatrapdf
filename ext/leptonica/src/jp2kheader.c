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
 * \file jp2kheader.c
 * <pre>
 *
 *      Read header
 *          l_int32          readHeaderJp2k()
 *          l_int32          freadHeaderJp2k()
 *          l_int32          readHeaderMemJp2k()
 *          l_int32          fgetJp2kResolution()
 *          l_int32          readResolutionMemJp2k()
 *
 *  Note: these function read image metadata from a jp2k file, without
 *  using any jp2k libraries.
 *
 *  To read and write jp2k data, using the OpenJPEG library
 *  (http://www.openjpeg.org), see jpegio.c.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include <math.h>
#include "allheaders.h"

#ifndef  NO_CONSOLE_IO
#define  DEBUG_CODEC        0
#endif  /* ~NO_CONSOLE_IO */

/* --------------------------------------------*/
#if  USE_JP2KHEADER   /* defined in environ.h */
/* --------------------------------------------*/

    /* a sanity check on the size read from file */
static const l_int32  MAX_JP2K_WIDTH = 100000;
static const l_int32  MAX_JP2K_HEIGHT = 100000;

/*--------------------------------------------------------------------*
 *                          Stream interface                          *
 *--------------------------------------------------------------------*/
/*!
 * \brief   readHeaderJp2k()
 *
 * \param[in]    filename
 * \param[out]   pw [optional]
 * \param[out]   ph [optional]
 * \param[out]   pbps [optional]  bits/sample
 * \param[out]   pspp [optional]  samples/pixel
 * \param[out]   pcodec [optional]  L_JP2_CODEC or L_J2K_CODEC
 * \return  0 if OK, 1 on error
 */
l_ok
readHeaderJp2k(const char *filename,
               l_int32    *pw,
               l_int32    *ph,
               l_int32    *pbps,
               l_int32    *pspp,
               l_int32    *pcodec)
{
l_int32  ret;
FILE    *fp;

    if (!filename)
        return ERROR_INT("filename not defined", __func__, 1);

    if ((fp = fopenReadStream(filename)) == NULL)
        return ERROR_INT_1("image file not found", filename, __func__, 1);
    ret = freadHeaderJp2k(fp, pw, ph, pbps, pspp, pcodec);
    fclose(fp);
    return ret;
}


/*!
 * \brief   freadHeaderJp2k()
 *
 * \param[in]    fp file stream opened for read
 * \param[out]   pw [optional]
 * \param[out]   ph [optional]
 * \param[out]   pbps [optional]  bits/sample
 * \param[out]   pspp [optional]  samples/pixel
 * \param[out]   pcodec [optional]  L_JP2_CODEC or L_J2K_CODEC
 * \return  0 if OK, 1 on error
 */
l_ok
freadHeaderJp2k(FILE     *fp,
                l_int32  *pw,
                l_int32  *ph,
                l_int32  *pbps,
                l_int32  *pspp,
                l_int32  *pcodec)
{
l_uint8  buf[120];  /* usually just need the first 80 bytes */
l_int32  nread, ret;

    if (!fp)
        return ERROR_INT("fp not defined", __func__, 1);

    rewind(fp);
    nread = fread(buf, 1, sizeof(buf), fp);
    if (nread != sizeof(buf))
        return ERROR_INT("read failure", __func__, 1);

    ret = readHeaderMemJp2k(buf, sizeof(buf), pw, ph, pbps, pspp, pcodec);
    rewind(fp);
    return ret;
}


/*!
 * \brief   readHeaderMemJp2k()
 *
 * \param[in]    data
 * \param[in]    size at least 80
 * \param[out]   pw [optional]
 * \param[out]   ph [optional]
 * \param[out]   pbps [optional]  bits/sample
 * \param[out]   pspp [optional]  samples/pixel
 * \param[out]   pcodec [optional]  L_JP2_CODEC or L_J2K_CODEC
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The ISO/IEC reference for jpeg2000 is
 *               http://www.jpeg.org/public/15444-1annexi.pdf
 *          and the file format syntax begins at page 127.
 *      (2) With a image file codec (L_JP2_CODEC), the Image Header Box
 *          begins with 'ihdr' = 0x69686472 in big-endian order.  This
 *          typically, but not always, starts on byte 44, with the
 *          big-endian data fields beginning at byte 48:
 *               h:    4 bytes
 *               w:    4 bytes
 *               spp:  2 bytes
 *               bps:  1 byte   (contains bps - 1)
 *      (3) With a codestream codec (L_J2K_CODEC), the first 4 bytes are
 *          0xff4fff51.  The fields for w and h appear to start on byte 8,
 *          and the fields for spp and bps appear to start on byte 40.
 * </pre>
 */
l_ok
readHeaderMemJp2k(const l_uint8  *data,
                  size_t          size,
                  l_int32        *pw,
                  l_int32        *ph,
                  l_int32        *pbps,
                  l_int32        *pspp,
                  l_int32        *pcodec)
{
l_int32  format, val, w, h, bps, spp, loc, found, index, codec;
l_uint8  ihdr[4] = {0x69, 0x68, 0x64, 0x72};  /* 'ihdr' */

    if (pw) *pw = 0;
    if (ph) *ph = 0;
    if (pbps) *pbps = 0;
    if (pspp) *pspp = 0;
    if (pcodec) *pcodec = 0;
    if (!data)
        return ERROR_INT("data not defined", __func__, 1);
    if (size < 120)
        return ERROR_INT("size < 80", __func__, 1);
    findFileFormatBuffer(data, &format);
    if (format != IFF_JP2)
        return ERROR_INT("not jp2 file", __func__, 1);

        /* Find beginning of the image metadata */
    if (!memcmp(data, "\xff\x4f\xff\x51", 4)) {   /* codestream */
        index = 8;
        codec = L_J2K_CODEC;
    } else {  /* file data with image header box 'ihdr' */
        arrayFindSequence(data, size, ihdr, 4, &loc, &found);
        if (!found)
            return ERROR_INT("image parameters not found", __func__, 1);
        index = loc + 4;
        codec = L_JP2_CODEC;
#if  DEBUG_CODEC
        if (loc != 44)
            L_INFO("Beginning of ihdr is at byte %d\n", __func__, loc);
#endif  /* DEBUG_CODEC */
    }
    if (pcodec) *pcodec = codec;

    if (codec == L_JP2_CODEC) {
        if (size < index + 4 * 3)
            return ERROR_INT("header size is too small", __func__, 1);
        val = *(l_uint32 *)(data + index);
        h = convertOnLittleEnd32(val);
        val = *(l_uint32 *)(data + index + 4);
        w = convertOnLittleEnd32(val);
        val = *(l_uint16 *)(data + index + 8);
        spp = convertOnLittleEnd16(val);
        bps = *(data + index + 10) + 1;
    } else {  /* codec == L_J2K_CODEC */
        if (size < index + 4 * 9)
            return ERROR_INT("header size is too small", __func__, 1);
        val = *(l_uint32 *)(data + index);
        w = convertOnLittleEnd32(val);
        val = *(l_uint32 *)(data + index + 4);
        h = convertOnLittleEnd32(val);
        val = *(l_uint16 *)(data + index + 32);
        spp = convertOnLittleEnd16(val);
        bps = *(data + index + 34) + 1;
    }
#if  DEBUG_CODEC
    lept_stderr("h = %d, w = %d, codec: %s, spp = %d, bps = %d\n", h, w,
                (codec == L_JP2_CODEC ? "jp2" : "j2k"), spp, bps);
#endif  /* DEBUG_CODEC */

    if (w < 1 || h < 1)
        return ERROR_INT("w and h must both be > 0", __func__, 1);
    if (w > MAX_JP2K_WIDTH || h > MAX_JP2K_HEIGHT)
        return ERROR_INT("unrealistically large sizes", __func__, 1);
    if (spp != 1 && spp != 3 && spp != 4)
        return ERROR_INT("spp must be in 1, 3 or 4", __func__, 1);
    if (bps != 8 && bps != 16)
        return ERROR_INT("bps must be 8 or 16", __func__, 1);
    if (pw) *pw = w;
    if (ph) *ph = h;
    if (pspp) *pspp = spp;
    if (pbps) *pbps = bps;
    return 0;
}


/*
 * \brief   fgetJp2kResolution()
 *
 * \param[in]   fp   file stream opened for read
 * \param[oui]  pxres   in ppi
 * \param[oui]  pyres   in ppi
 * \return  0 if found, 1 if not found or on error
 *
 * <pre>
 * Notes:
 *      (1) If the capture resolution field is not set, this is not an error;
 *          the returned resolution values are 0 (designating 'unknown').
 *      (2) Side-effect: this rewinds the stream.
 *      (3) The capture resolution box is optional in the jp2 spec, and
 *          it is usually not written.
 *      (4) The big-endian data fields that follow the 4 bytes of 'resc' are:
 *             ynum:    2 bytes
 *             ydenom:  2 bytes
 *             xnum:    2 bytes
 *             xdenom:  2 bytes
 *             yexp:    1 byte
 *             xexp:    1 byte
 * </pre>
 */
l_int32
fgetJp2kResolution(FILE     *fp,
                   l_int32  *pxres,
                   l_int32  *pyres)
{
l_uint8  *data;
size_t    nbytes;
l_ok      ok;

    if (!fp)
        return ERROR_INT("stream not opened", __func__, 1);

    rewind(fp);
    data = l_binaryReadStream(fp, &nbytes);
    rewind(fp);

    ok = readResolutionMemJp2k(data, nbytes, pxres, pyres);

    LEPT_FREE(data);
    return ok;
}


/*!
 * \brief   readResolutionMemJp2k()
 *
 * \param[in]   data    const; jp2k-encoded
 * \param[in]   nbytes  of data
 * \param[out]  pxres   [optional]
 * \param[out]  pyres   [optional]
 * \return  0 if OK, 1 on error
 */
l_ok
readResolutionMemJp2k(const l_uint8  *data,
                      size_t          nbytes,
                      l_int32        *pxres,
                      l_int32        *pyres)
{
l_uint8    xexp, yexp;
l_uint16   xnum, ynum, xdenom, ydenom;  /* these jp2k fields are 2-byte */
l_int32    loc, found;
l_uint8    resc[4] = {0x72, 0x65, 0x73, 0x63};  /* 'resc' */
l_float64  xres, yres, maxres;

    if (pxres) *pxres = 0;
    if (pyres) *pyres = 0;
    if (!pxres || !pyres)
        return ERROR_INT("&xres and &yres not both defined", __func__, 1);

        /* Search for the start of the first capture resolution box: 'resc' */
    arrayFindSequence(data, nbytes, resc, 4, &loc, &found);
    if (!found) {
        L_WARNING("image resolution not found\n", __func__);
        return 1;
    }
    if (nbytes < 80 || loc >= nbytes - 13) {
        L_WARNING("image resolution found without enough space\n", __func__);
        return 1;
    }

        /* Extract the fields and calculate the resolution in pixels/meter.
         * See section 1.5.3.7.1 of JPEG 2000 ISO/IEC 15444-1 spec.  */
    ynum = data[loc + 5] << 8 | data[loc + 4];
    ynum = convertOnLittleEnd16(ynum);
    ydenom = data[loc + 7] << 8 | data[loc + 6];
    ydenom = convertOnLittleEnd16(ydenom);
    xnum = data[loc + 9] << 8 | data[loc + 8];
    xnum = convertOnLittleEnd16(xnum);
    xdenom = data[loc + 11] << 8 | data[loc + 10];
    xdenom = convertOnLittleEnd16(xdenom);
    if (ydenom == 0 || xdenom == 0) {
        L_WARNING("bad data: ydenom or xdenom is 0\n", __func__);
        return 1;
    }
    yexp = data[loc + 12];
    xexp = data[loc + 13];
    yres = ((l_float64)ynum / (l_float64)ydenom) * pow(10.0, (l_float64)yexp);
    xres = ((l_float64)xnum / (l_float64)xdenom) * pow(10.0, (l_float64)xexp);

        /* Convert from pixels/meter to ppi */
    yres *= (300.0 / 11811.0);
    xres *= (300.0 / 11811.0);

        /* Sanity check for bad data */
    maxres = 100000.0;  /* ppi */
    if (xres > maxres || yres > maxres) {
        L_WARNING("ridiculously large resolution\n", __func__);
    } else {
        *pyres = (l_int32)(yres + 0.5);
        *pxres = (l_int32)(xres + 0.5);
    }

    return 0;
}

/* --------------------------------------------*/
#endif  /* USE_JP2KHEADER */
