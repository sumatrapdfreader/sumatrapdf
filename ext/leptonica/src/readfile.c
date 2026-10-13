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
 * \file readfile.c:  reads image on file into memory
 * <pre>
 *
 *      Top-level functions for reading images from file
 *           PIXA      *pixaReadFiles()
 *           PIXA      *pixaReadFilesSA()
 *           PIX       *pixRead()
 *           PIX       *pixReadWithHint()
 *           PIX       *pixReadIndexed()
 *           PIX       *pixReadStream()
 *
 *      Read header information from file
 *           l_int32    pixReadHeader()
 *
 *      Format finders
 *           l_int32    findFileFormat()
 *           l_int32    findFileFormatStream()
 *           l_int32    findFileFormatBuffer()
 *           l_int32    fileFormatIsTiff()
 *
 *      Read from memory
 *           PIX       *pixReadMem()
 *           l_int32    pixReadHeaderMem()
 *
 *      Output image file information
 *           void       writeImageFileInfo()
 *
 *      Test function for I/O with different formats
 *           l_int32    ioFormatTest()
 *
 *  Supported file formats:
 *  (1) Reading is supported without any external libraries:
 *          bmp
 *          pnm   (including pbm, pgm, etc)
 *          spix  (raw serialized)
 *  (2) Reading is supported with installation of external libraries:
 *          png
 *          jpg   (standard jfif version)
 *          tiff  (including most varieties of compression)
 *          gif
 *          webp
 *          jp2 (jpeg 2000)
 *  (3) Other file types will get an "unknown format" error.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"

    /* Output files for ioFormatTest(). */
static const char *FILE_BMP  =  "/tmp/lept/format/file.bmp";
static const char *FILE_PNG  =  "/tmp/lept/format/file.png";
static const char *FILE_PNM  =  "/tmp/lept/format/file.pnm";
static const char *FILE_G3   =  "/tmp/lept/format/file_g3.tif";
static const char *FILE_G4   =  "/tmp/lept/format/file_g4.tif";
static const char *FILE_RLE  =  "/tmp/lept/format/file_rle.tif";
static const char *FILE_PB   =  "/tmp/lept/format/file_packbits.tif";
static const char *FILE_LZW  =  "/tmp/lept/format/file_lzw.tif";
static const char *FILE_ZIP  =  "/tmp/lept/format/file_zip.tif";
static const char *FILE_TIFF_JPEG =  "/tmp/lept/format/file_jpeg.tif";
static const char *FILE_TIFF =  "/tmp/lept/format/file.tif";
static const char *FILE_JPG  =  "/tmp/lept/format/file.jpg";
static const char *FILE_GIF  =  "/tmp/lept/format/file.gif";
static const char *FILE_WEBP =  "/tmp/lept/format/file.webp";
static const char *FILE_JP2K =  "/tmp/lept/format/file.jp2";

    /* There are two jp2 formats, and two codecs associated with them:
     *    OPJ_CODEC_J2K (L_J2K_CODEC) is associated with JP2K_CODESTREAM
     *    OPJ_CODEC_JP2 (L_JP2_CODEC) is associated with JP2K_IMAGE_DATA    */
static const unsigned char JP2K_CODESTREAM[4] = { 0xff, 0x4f, 0xff, 0x51 };
static const unsigned char JP2K_IMAGE_DATA[12] = { 0x00, 0x00, 0x00, 0x0c,
                                                   0x6a, 0x50, 0x20, 0x20,
                                                   0x0d, 0x0a, 0x87, 0x0a };


/*---------------------------------------------------------------------*
 *          Top-level functions for reading images from file           *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaReadFiles()
 *
 * \param[in]    dirname
 * \param[in]    substr   [optional] substring filter on filenames; can be null
 * \return  pixa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) %dirname is the full path for the directory.
 *      (2) %substr is the part of the file name (excluding
 *          the directory) that is to be matched.  All matching
 *          filenames are read into the Pixa.  If substr is NULL,
 *          all filenames are read into the Pixa.
 * </pre>
 */
PIXA *
pixaReadFiles(const char  *dirname,
              const char  *substr)
{
PIXA    *pixa;
SARRAY  *sa;

    if (!dirname)
        return (PIXA *)ERROR_PTR("dirname not defined", __func__, NULL);

    if ((sa = getSortedPathnamesInDirectory(dirname, substr, 0, 0)) == NULL)
        return (PIXA *)ERROR_PTR("sa not made", __func__, NULL);

    pixa = pixaReadFilesSA(sa);
    sarrayDestroy(&sa);
    return pixa;
}


/*!
 * \brief   pixaReadFilesSA()
 *
 * \param[in]    sa     full pathnames for all files
 * \return  pixa, or NULL on error
 */
PIXA *
pixaReadFilesSA(SARRAY  *sa)
{
char    *str;
l_int32  i, n;
PIX     *pix;
PIXA    *pixa;

    if (!sa)
        return (PIXA *)ERROR_PTR("sa not defined", __func__, NULL);

    n = sarrayGetCount(sa);
    pixa = pixaCreate(n);
    for (i = 0; i < n; i++) {
        str = sarrayGetString(sa, i, L_NOCOPY);
        if ((pix = pixRead(str)) == NULL) {
            L_WARNING("pix not read from file %s\n", __func__, str);
            continue;
        }
        pixaAddPix(pixa, pix, L_INSERT);
    }

    return pixa;
}


/*!
 * \brief   pixRead()
 *
 * \param[in]    filename    with full pathname or in local directory
 * \return  pix if OK; NULL on error
 *
 * <pre>
 * Notes:
 *      (1) See at top of file for supported formats.
 * </pre>
 */
PIX *
pixRead(const char  *filename)
{
FILE  *fp;
PIX   *pix;

    if (!filename)
        return (PIX *)ERROR_PTR("filename not defined", __func__, NULL);

    if ((fp = fopenReadStream(filename)) == NULL)
        return (PIX*)ERROR_PTR_1("image file not found",
                                 filename, __func__, NULL);
    pix = pixReadStream(fp, 0);
    fclose(fp);
    if (!pix)
        return (PIX *)ERROR_PTR_1("pix not read", filename, __func__, NULL);
    return pix;
}


/*!
 * \brief   pixReadWithHint()
 *
 * \param[in]    filename    with full pathname or in local directory
 * \param[in]    hint        bitwise OR of L_HINT_* values for jpeg;
 *                           use 0 for no hint
 * \return  pix if OK; NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The hint is not binding, but may be used to optimize jpeg decoding.
 *          Use 0 for no hinting.
 * </pre>
 */
PIX *
pixReadWithHint(const char  *filename,
                l_int32      hint)
{
FILE  *fp;
PIX   *pix;

    if (!filename)
        return (PIX *)ERROR_PTR("filename not defined", __func__, NULL);

    if ((fp = fopenReadStream(filename)) == NULL)
        return (PIX *)ERROR_PTR_1("image file not found",
                                  filename, __func__, NULL);
    pix = pixReadStream(fp, hint);
    fclose(fp);

    if (!pix)
        return (PIX *)ERROR_PTR_1("image not returned",
                                  filename, __func__, NULL);
    return pix;
}


/*!
 * \brief   pixReadIndexed()
 *
 * \param[in]    sa      string array of full pathnames
 * \param[in]    index   into pathname array
 * \return  pix if OK; null if not found
 *
 * <pre>
 * Notes:
 *      (1) This function is useful for selecting image files from a
 *          directory, where the integer %index is embedded into
 *          the file name.
 *      (2) This is typically done by generating the sarray using
 *          getNumberedPathnamesInDirectory(), so that the %index
 *          pathname would have the number %index in it.  The size
 *          of the sarray should be the largest number (plus 1) appearing
 *          in the file names, respecting the constraints in the
 *          call to getNumberedPathnamesInDirectory().
 *      (3) Consequently, for some indices into the sarray, there may
 *          be no pathnames in the directory containing that number.
 *          By convention, we place empty C strings ("") in those
 *          locations in the sarray, and it is not an error if such
 *          a string is encountered and no pix is returned.
 *          Therefore, the caller must verify that a pix is returned.
 *      (4) See convertSegmentedPagesToPS() in src/psio1.c for an
 *          example of usage.
 * </pre>
 */
PIX *
pixReadIndexed(SARRAY  *sa,
               l_int32  index)
{
char    *fname;
l_int32  n;
PIX     *pix;

    if (!sa)
        return (PIX *)ERROR_PTR("sa not defined", __func__, NULL);
    n = sarrayGetCount(sa);
    if (index < 0 || index >= n)
        return (PIX *)ERROR_PTR("index out of bounds", __func__, NULL);

    fname = sarrayGetString(sa, index, L_NOCOPY);
    if (fname[0] == '\0')
        return NULL;

    if ((pix = pixRead(fname)) == NULL) {
        L_ERROR("pix not read from file %s\n", __func__, fname);
        return NULL;
    }

    return pix;
}


/*!
 * \brief   pixReadStream()
 *
 * \param[in]    fp      file stream
 * \param[in]    hint    bitwise OR of L_HINT_* values for jpeg; 0 for no hint
 * \return  pix if OK; NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The hint only applies to jpeg.
 * </pre>
 */
PIX *
pixReadStream(FILE    *fp,
              l_int32  hint)
{
l_int32   format, ret, valid;
l_uint8  *comment;
PIX      *pix;
PIXCMAP  *cmap;

    if (!fp)
        return (PIX *)ERROR_PTR("stream not defined", __func__, NULL);
    pix = NULL;

    findFileFormatStream(fp, &format);
    switch (format)
    {
    case IFF_BMP:
        if ((pix = pixReadStreamBmp(fp)) == NULL )
            return (PIX *)ERROR_PTR( "bmp: no pix returned", __func__, NULL);
        break;

    case IFF_JFIF_JPEG:
        if ((pix = pixReadStreamJpeg(fp, 0, 1, NULL, hint)) == NULL)
            return (PIX *)ERROR_PTR( "jpeg: no pix returned", __func__, NULL);
        ret = fgetJpegComment(fp, &comment);
        if (!ret && comment)
            pixSetText(pix, (char *)comment);
        LEPT_FREE(comment);
        break;

    case IFF_PNG:
        if ((pix = pixReadStreamPng(fp)) == NULL)
            return (PIX *)ERROR_PTR("png: no pix returned", __func__, NULL);
        break;

    case IFF_TIFF:
    case IFF_TIFF_PACKBITS:
    case IFF_TIFF_RLE:
    case IFF_TIFF_G3:
    case IFF_TIFF_G4:
    case IFF_TIFF_LZW:
    case IFF_TIFF_ZIP:
    case IFF_TIFF_JPEG:
        if ((pix = pixReadStreamTiff(fp, 0)) == NULL)  /* page 0 by default */
            return (PIX *)ERROR_PTR("tiff: no pix returned", __func__, NULL);
        break;

    case IFF_PNM:
        if ((pix = pixReadStreamPnm(fp)) == NULL)
            return (PIX *)ERROR_PTR("pnm: no pix returned", __func__, NULL);
        break;

    case IFF_GIF:
        if ((pix = pixReadStreamGif(fp)) == NULL)
            return (PIX *)ERROR_PTR("gif: no pix returned", __func__, NULL);
        break;

    case IFF_JP2:
        if ((pix = pixReadStreamJp2k(fp, 1, NULL, 0, 0)) == NULL)
            return (PIX *)ERROR_PTR("jp2: no pix returned", __func__, NULL);
        break;

    case IFF_WEBP:
        if ((pix = pixReadStreamWebP(fp)) == NULL)
            return (PIX *)ERROR_PTR("webp: no pix returned", __func__, NULL);
        break;

    case IFF_PS:
        L_ERROR("PostScript reading is not supported\n", __func__);
        return NULL;

    case IFF_LPDF:
        L_ERROR("Pdf reading is not supported\n", __func__);
        return NULL;

    case IFF_SPIX:
        if ((pix = pixReadStreamSpix(fp)) == NULL)
            return (PIX *)ERROR_PTR("spix: no pix returned", __func__, NULL);
        break;

    case IFF_UNKNOWN:
        return (PIX *)ERROR_PTR( "Unknown format: no pix returned",
                __func__, NULL);
    }

    if (pix) {
        pixSetInputFormat(pix, format);
        if ((cmap = pixGetColormap(pix))) {
            pixcmapIsValid(cmap, pix, &valid);
            if (!valid) {
                pixDestroy(&pix);
                return (PIX *)ERROR_PTR("invalid colormap", __func__, NULL);
            }
        }
    }
    return pix;
}



/*---------------------------------------------------------------------*
 *                     Read header information from file               *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixReadHeader()
 *
 * \param[in]    filename    with full pathname or in local directory
 * \param[out]   pformat     [optional] file format
 * \param[out]   pw, ph      [optional] width and height
 * \param[out]   pbps        [optional] bits/sample
 * \param[out]   pspp        [optional] samples/pixel 1, 3 or 4
 * \param[out]   piscmap     [optional] 1 if cmap exists; 0 otherwise
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This reads the actual headers for jpeg, png, tiff and pnm.
 *          For bmp and gif, we cheat and read the entire file into a pix,
 *          from which we extract the "header" information.
 * </pre>
 */
l_ok
pixReadHeader(const char  *filename,
              l_int32     *pformat,
              l_int32     *pw,
              l_int32     *ph,
              l_int32     *pbps,
              l_int32     *pspp,
              l_int32     *piscmap)
{
l_int32  format, ret, w, h, d, bps, spp, iscmap;
l_int32  type;  /* ignored */
FILE    *fp;
PIX     *pix;

    if (pw) *pw = 0;
    if (ph) *ph = 0;
    if (pbps) *pbps = 0;
    if (pspp) *pspp = 0;
    if (piscmap) *piscmap = 0;
    if (pformat) *pformat = 0;
    iscmap = 0;  /* init to false */
    if (!filename)
        return ERROR_INT("filename not defined", __func__, 1);

    if ((fp = fopenReadStream(filename)) == NULL)
        return ERROR_INT_1("image file not found", filename, __func__, 1);
    findFileFormatStream(fp, &format);
    fclose(fp);

    switch (format)
    {
    case IFF_BMP:  /* cheating: reading the entire file */
        if ((pix = pixRead(filename)) == NULL)
            return ERROR_INT_1( "bmp: pix not read", filename, __func__, 1);
        pixGetDimensions(pix, &w, &h, &d);
        bps = (d == 32) ? 8 : d;
        spp = pixGetSpp(pix);
        iscmap = (pixGetColormap(pix)) ? 1 : 0;
        pixDestroy(&pix);
        break;

    case IFF_JFIF_JPEG:
        ret = readHeaderJpeg(filename, &w, &h, &spp, NULL, NULL);
        bps = 8;
        if (ret)
            return ERROR_INT_1("jpeg: no header info returned",
                               filename, __func__, 1);
        break;

    case IFF_PNG:
        ret = readHeaderPng(filename, &w, &h, &bps, &spp, &iscmap);
        if (ret)
            return ERROR_INT_1("png: no header info returned",
                               filename, __func__, 1);
        break;

    case IFF_TIFF:
    case IFF_TIFF_PACKBITS:
    case IFF_TIFF_RLE:
    case IFF_TIFF_G3:
    case IFF_TIFF_G4:
    case IFF_TIFF_LZW:
    case IFF_TIFF_ZIP:
    case IFF_TIFF_JPEG:
            /* Reading page 0 by default; possibly redefine format */
        ret = readHeaderTiff(filename, 0, &w, &h, &bps, &spp, NULL, &iscmap,
                             &format);
        if (ret)
            return ERROR_INT_1("tiff: no header info returned",
                               filename, __func__, 1);
        break;

    case IFF_PNM:
        ret = readHeaderPnm(filename, &w, &h, &d, &type, &bps, &spp);
        if (ret)
            return ERROR_INT_1("pnm: no header info returned",
                               filename, __func__, 1);
        break;

    case IFF_GIF:  /* cheating: reading the entire file */
        if ((pix = pixRead(filename)) == NULL)
            return ERROR_INT_1( "gif: pix not read", filename, __func__, 1);
        pixGetDimensions(pix, &w, &h, &d);
        pixDestroy(&pix);
        iscmap = 1;  /* always colormapped; max 256 colors */
        spp = 1;
        bps = d;
        break;

    case IFF_JP2:
        ret = readHeaderJp2k(filename, &w, &h, &bps, &spp, NULL);
        break;

    case IFF_WEBP:
        if (readHeaderWebP(filename, &w, &h, &spp))
            return ERROR_INT_1("webp: no header info returned",
                               filename, __func__, 1);
        bps = 8;
        break;

    case IFF_PS:
        if (pformat) *pformat = format;
        return ERROR_INT("PostScript reading is not supported\n", __func__, 1);

    case IFF_LPDF:
        if (pformat) *pformat = format;
        return ERROR_INT("Pdf reading is not supported\n", __func__, 1);

    case IFF_SPIX:
        ret = readHeaderSpix(filename, &w, &h, &bps, &spp, &iscmap);
        if (ret)
            return ERROR_INT_1("spix: no header info returned",
                               filename, __func__, 1);
        break;

    case IFF_UNKNOWN:
        return ERROR_INT_1("unknown format in file", filename, __func__, 1);
    }

    if (pw) *pw = w;
    if (ph) *ph = h;
    if (pbps) *pbps = bps;
    if (pspp) *pspp = spp;
    if (piscmap) *piscmap = iscmap;
    if (pformat) *pformat = format;
    return 0;
}


/*---------------------------------------------------------------------*
 *                            Format finders                           *
 *---------------------------------------------------------------------*/
/*!
 * \brief   findFileFormat()
 *
 * \param[in]    filename
 * \param[out]   pformat    found format
 * \return  0 if OK, 1 on error or if format is not recognized
 */
l_ok
findFileFormat(const char  *filename,
               l_int32     *pformat)
{
l_int32  ret;
FILE    *fp;

    if (!pformat)
        return ERROR_INT("&format not defined", __func__, 1);
    *pformat = IFF_UNKNOWN;
    if (!filename)
        return ERROR_INT("filename not defined", __func__, 1);

    if ((fp = fopenReadStream(filename)) == NULL)
        return ERROR_INT_1("image file not found", filename, __func__, 1);
    ret = findFileFormatStream(fp, pformat);
    fclose(fp);
    return ret;
}


/*!
 * \brief   findFileFormatStream()
 *
 * \param[in]    fp        file stream
 * \param[out]   pformat   found format
 * \return  0 if OK, 1 on error or if format is not recognized
 *
 * <pre>
 * Notes:
 *      (1) Important: Side effect -- this resets fp to BOF.
 * </pre>
 */
l_ok
findFileFormatStream(FILE     *fp,
                     l_int32  *pformat)
{
l_uint8  firstbytes[13];
l_int32  format;

    if (!pformat)
        return ERROR_INT("&format not defined", __func__, 1);
    *pformat = IFF_UNKNOWN;
    if (!fp)
        return ERROR_INT("stream not defined", __func__, 1);

    rewind(fp);
    if (fnbytesInFile(fp) < 12)
        return ERROR_INT("truncated file", __func__, 1);

    if (fread(&firstbytes, 1, 12, fp) != 12)
        return ERROR_INT("failed to read first 12 bytes of file", __func__, 1);
    firstbytes[12] = 0;
    rewind(fp);

    findFileFormatBuffer(firstbytes, &format);
    if (format == IFF_TIFF) {
        findTiffCompression(fp, &format);
        rewind(fp);
    }
    *pformat = format;
    if (format == IFF_UNKNOWN)
        return 1;
    else
        return 0;
}


/*!
 * \brief   findFileFormatBuffer()
 *
 * \param[in]    buf       byte buffer at least 12 bytes in size; we can't check
 * \param[out]   pformat   found format
 * \return  0 if OK, 1 on error or if format is not recognized
 *
 * <pre>
 * Notes:
 *      (1) This determines the file format from the first 12 bytes in
 *          the compressed data stream, which are stored in memory.
 *      (2) For tiff files, this returns IFF_TIFF.  The specific tiff
 *          compression is then determined using findTiffCompression().
 * </pre>
 */
l_ok
findFileFormatBuffer(const l_uint8  *buf,
                     l_int32        *pformat)
{
l_uint16  twobytepw;

    if (!pformat)
        return ERROR_INT("&format not defined", __func__, 1);
    *pformat = IFF_UNKNOWN;
    if (!buf)
        return ERROR_INT("byte buffer not defined", __func__, 0);

        /* Check the bmp and tiff 2-byte header ids */
    ((char *)(&twobytepw))[0] = buf[0];
    ((char *)(&twobytepw))[1] = buf[1];

    if (convertOnBigEnd16(twobytepw) == BMP_ID) {
        *pformat = IFF_BMP;
        return 0;
    }

    if (twobytepw == TIFF_BIGEND_ID || twobytepw == TIFF_LITTLEEND_ID) {
        *pformat = IFF_TIFF;
        return 0;
    }

        /* Check for the p*m 2-byte header ids */
    if ((buf[0] == 'P' && buf[1] == '4') || /* newer packed */
        (buf[0] == 'P' && buf[1] == '1')) {  /* old ASCII format */
        *pformat = IFF_PNM;
        return 0;
    }

    if ((buf[0] == 'P' && buf[1] == '5') || /* newer */
        (buf[0] == 'P' && buf[1] == '2')) {  /* old */
        *pformat = IFF_PNM;
        return 0;
    }

    if ((buf[0] == 'P' && buf[1] == '6') || /* newer */
        (buf[0] == 'P' && buf[1] == '3')) {  /* old */
        *pformat = IFF_PNM;
        return 0;
    }

    if (buf[0] == 'P' && buf[1] == '7') {  /* new arbitrary (PAM) */
        *pformat = IFF_PNM;
        return 0;
    }

        /*  Consider the first 11 bytes of the standard JFIF JPEG header:
         *    - The first two bytes are the most important:  0xffd8.
         *    - The next two bytes are the jfif marker: 0xffe0.
         *      Not all jpeg files have this marker.
         *    - The next two bytes are the header length.
         *    - The next 5 bytes are a null-terminated string.
         *      For JFIF, the string is "JFIF", naturally.  For others it
         *      can be "Exif" or just about anything else.
         *    - Because of all this variability, we only check the first
         *      two byte marker.  All jpeg files are identified as
         *      IFF_JFIF_JPEG.  */
    if (buf[0] == 0xff && buf[1] == 0xd8) {
        *pformat = IFF_JFIF_JPEG;
        return 0;
    }

        /* Check for the 8 byte PNG signature (png_signature in png.c):
         *       {137, 80, 78, 71, 13, 10, 26, 10}      */
    if (buf[0] == 137 && buf[1] == 80  && buf[2] == 78  && buf[3] == 71  &&
        buf[4] == 13  && buf[5] == 10  && buf[6] == 26  && buf[7] == 10) {
        *pformat = IFF_PNG;
        return 0;
    }

        /* Look for "GIF87a" or "GIF89a" */
    if (buf[0] == 'G' && buf[1] == 'I' && buf[2] == 'F' && buf[3] == '8' &&
        (buf[4] == '7' || buf[4] == '9') && buf[5] == 'a') {
        *pformat = IFF_GIF;
        return 0;
    }

        /* Check for both types of jp2k file */
    if (memcmp(buf, JP2K_CODESTREAM, 4) == 0 ||
        memcmp(buf, JP2K_IMAGE_DATA, 12) == 0) {
        *pformat = IFF_JP2;
        return 0;
    }

        /* Check for webp */
    if (buf[0] == 'R' && buf[1] == 'I' && buf[2] == 'F' && buf[3] == 'F' &&
        buf[8] == 'W' && buf[9] == 'E' && buf[10] == 'B' && buf[11] == 'P') {
        *pformat = IFF_WEBP;
        return 0;
    }

        /* Check for ps */
    if (buf[0] == '%' && buf[1] == '!' && buf[2] == 'P' && buf[3] == 'S' &&
        buf[4] == '-' && buf[5] == 'A' && buf[6] == 'd' && buf[7] == 'o' &&
        buf[8] == 'b' && buf[9] == 'e') {
        *pformat = IFF_PS;
        return 0;
    }

        /* Check for pdf */
    if (buf[0] == '%' && buf[1] == 'P' && buf[2] == 'D' && buf[3] == 'F' &&
        buf[4] == '-' && buf[5] == '1') {
        *pformat = IFF_LPDF;
        return 0;
    }

        /* Check for "spix" serialized pix */
    if (buf[0] == 's' && buf[1] == 'p' && buf[2] == 'i' && buf[3] == 'x') {
        *pformat = IFF_SPIX;
        return 0;
    }

        /* File format identifier not found; unknown */
    return 1;
}


/*!
 * \brief   fileFormatIsTiff()
 *
 * \param[in]    fp    file stream
 * \return  1 if file is tiff; 0 otherwise or on error
 */
l_int32
fileFormatIsTiff(FILE  *fp)
{
l_int32  format;

    if (!fp)
        return ERROR_INT("stream not defined", __func__, 0);

    findFileFormatStream(fp, &format);
    if (format == IFF_TIFF || format == IFF_TIFF_PACKBITS ||
        format == IFF_TIFF_RLE || format == IFF_TIFF_G3 ||
        format == IFF_TIFF_G4 || format == IFF_TIFF_LZW ||
        format == IFF_TIFF_ZIP || format == IFF_TIFF_JPEG)
        return 1;
    else
        return 0;
}


/*---------------------------------------------------------------------*
 *                            Read from memory                         *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixReadMem()
 *
 * \param[in]    data    const; encoded
 * \param[in]    size    size of data
 * \return  pix, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is a variation of pixReadStream(), where the data is read
 *          from a memory buffer rather than a file.
 *      (2) On Windows, this only reads tiff formatted files directly from
 *          memory.  For other formats, it writes to a temp file and
 *          decompresses from file.
 *      (3) findFileFormatBuffer() requires up to 12 bytes to decide on
 *          the format.  That determines the constraint here.  But in
 *          fact the data must contain the entire compressed string for
 *          the image.
 * </pre>
 */
PIX *
pixReadMem(const l_uint8  *data,
           size_t          size)
{
l_int32   format, valid;
PIX      *pix;
PIXCMAP  *cmap;

    if (!data)
        return (PIX *)ERROR_PTR("data not defined", __func__, NULL);
    if (size < 12)
        return (PIX *)ERROR_PTR("size < 12", __func__, NULL);
    pix = NULL;

    findFileFormatBuffer(data, &format);
    switch (format)
    {
    case IFF_BMP:
        if ((pix = pixReadMemBmp(data, size)) == NULL )
            return (PIX *)ERROR_PTR( "bmp: no pix returned", __func__, NULL);
        break;

    case IFF_JFIF_JPEG:
        if ((pix = pixReadMemJpeg(data, size, 0, 1, NULL, 0)) == NULL)
            return (PIX *)ERROR_PTR( "jpeg: no pix returned", __func__, NULL);
        break;

    case IFF_PNG:
        if ((pix = pixReadMemPng(data, size)) == NULL)
            return (PIX *)ERROR_PTR("png: no pix returned", __func__, NULL);
        break;

    case IFF_TIFF:
    case IFF_TIFF_PACKBITS:
    case IFF_TIFF_RLE:
    case IFF_TIFF_G3:
    case IFF_TIFF_G4:
    case IFF_TIFF_LZW:
    case IFF_TIFF_ZIP:
            /* Reading page 0 by default */
        if ((pix = pixReadMemTiff(data, size, 0)) == NULL)
            return (PIX *)ERROR_PTR("tiff: no pix returned", __func__, NULL);
        break;

    case IFF_PNM:
        if ((pix = pixReadMemPnm(data, size)) == NULL)
            return (PIX *)ERROR_PTR("pnm: no pix returned", __func__, NULL);
        break;

    case IFF_GIF:
        if ((pix = pixReadMemGif(data, size)) == NULL)
            return (PIX *)ERROR_PTR("gif: no pix returned", __func__, NULL);
        break;

    case IFF_JP2:
        if ((pix = pixReadMemJp2k(data, size, 1, NULL, 0, 0)) == NULL)
            return (PIX *)ERROR_PTR("jp2k: no pix returned", __func__, NULL);
        break;

    case IFF_WEBP:
        if ((pix = pixReadMemWebP(data, size)) == NULL)
            return (PIX *)ERROR_PTR("webp: no pix returned", __func__, NULL);
        break;

    case IFF_PS:
        L_ERROR("PostScript reading is not supported\n", __func__);
        return NULL;

    case IFF_LPDF:
        L_ERROR("Pdf reading is not supported\n", __func__);
        return NULL;

    case IFF_SPIX:
        if ((pix = pixReadMemSpix(data, size)) == NULL)
            return (PIX *)ERROR_PTR("spix: no pix returned", __func__, NULL);
        break;

    case IFF_UNKNOWN:
        return (PIX *)ERROR_PTR("Unknown format: no pix returned",
                __func__, NULL);
    }

        /* Set the input format.  For tiff reading from memory we lose
         * the actual input format; for 1 bpp, default to G4.  Also
         * verify that the colormap is valid.  */
    if (pix) {
        if (format == IFF_TIFF && pixGetDepth(pix) == 1)
            format = IFF_TIFF_G4;
        pixSetInputFormat(pix, format);
        if ((cmap = pixGetColormap(pix))) {
            pixcmapIsValid(cmap, pix, &valid);
            if (!valid) {
                pixDestroy(&pix);
                return (PIX *)ERROR_PTR("invalid colormap", __func__, NULL);
            }
        }
        pixSetPadBits(pix, 0);
    }
    return pix;
}


/*!
 * \brief   pixReadHeaderMem()
 *
 * \param[in]    data       const; encoded
 * \param[in]    size       size of data
 * \param[out]   pformat    [optional] image format
 * \param[out]   pw, ph     [optional] width and height
 * \param[out]   pbps       [optional] bits/sample
 * \param[out]   pspp       [optional] samples/pixel 1, 3 or 4
 * \param[out]   piscmap    [optional] 1 if cmap exists; 0 otherwise
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This reads the actual headers for jpeg, png, tiff, jp2k and pnm.
 *          For bmp and gif, we cheat and read all the data into a pix,
 *          from which we extract the "header" information.
 *      (2) The amount of data required depends on the format.  For
 *          png, it requires less than 30 bytes, but for jpeg it can
 *          require most of the compressed file.  In practice, the data
 *          is typically the entire compressed file in memory.
 *      (3) findFileFormatBuffer() requires up to 12 bytes to decide on
 *          the format, which we require.
 * </pre>
 */
l_ok
pixReadHeaderMem(const l_uint8  *data,
                 size_t          size,
                 l_int32        *pformat,
                 l_int32        *pw,
                 l_int32        *ph,
                 l_int32        *pbps,
                 l_int32        *pspp,
                 l_int32        *piscmap)
{
l_int32  format, ret, w, h, d, bps, spp, iscmap;
l_int32  type;  /* not used */
PIX     *pix;

    if (pw) *pw = 0;
    if (ph) *ph = 0;
    if (pbps) *pbps = 0;
    if (pspp) *pspp = 0;
    if (piscmap) *piscmap = 0;
    if (pformat) *pformat = 0;
    iscmap = 0;  /* init to false */
    if (!data)
        return ERROR_INT("data not defined", __func__, 1);
    if (size < 12)
        return ERROR_INT("size < 12", __func__, 1);

    findFileFormatBuffer(data, &format);

    switch (format)
    {
    case IFF_BMP:  /* cheating: read the pix */
        if ((pix = pixReadMemBmp(data, size)) == NULL)
            return ERROR_INT( "bmp: pix not read", __func__, 1);
        pixGetDimensions(pix, &w, &h, &d);
        pixDestroy(&pix);
        bps = (d == 32) ? 8 : d;
        spp = (d == 32) ? 3 : 1;
        break;

    case IFF_JFIF_JPEG:
        ret = readHeaderMemJpeg(data, size, &w, &h, &spp, NULL, NULL);
        bps = 8;
        if (ret)
            return ERROR_INT( "jpeg: no header info returned", __func__, 1);
        break;

    case IFF_PNG:
        ret = readHeaderMemPng(data, size, &w, &h, &bps, &spp, &iscmap);
        if (ret)
            return ERROR_INT( "png: no header info returned", __func__, 1);
        break;

    case IFF_TIFF:
    case IFF_TIFF_PACKBITS:
    case IFF_TIFF_RLE:
    case IFF_TIFF_G3:
    case IFF_TIFF_G4:
    case IFF_TIFF_LZW:
    case IFF_TIFF_ZIP:
    case IFF_TIFF_JPEG:
            /* Reading page 0 by default; possibly redefine format */
        ret = readHeaderMemTiff(data, size, 0, &w, &h, &bps, &spp,
                                NULL, &iscmap, &format);
        if (ret)
            return ERROR_INT( "tiff: no header info returned", __func__, 1);
        break;

    case IFF_PNM:
        ret = readHeaderMemPnm(data, size, &w, &h, &d, &type, &bps, &spp);
        if (ret)
            return ERROR_INT( "pnm: no header info returned", __func__, 1);
        break;

    case IFF_GIF:  /* cheating: read the pix */
        if ((pix = pixReadMemGif(data, size)) == NULL)
            return ERROR_INT( "gif: pix not read", __func__, 1);
        pixGetDimensions(pix, &w, &h, &d);
        pixDestroy(&pix);
        iscmap = 1;  /* always colormapped; max 256 colors */
        spp = 1;
        bps = d;
        break;

    case IFF_JP2:
        ret = readHeaderMemJp2k(data, size, &w, &h, &bps, &spp, NULL);
        break;

    case IFF_WEBP:
        bps = 8;
        ret = readHeaderMemWebP(data, size, &w, &h, &spp);
        break;

    case IFF_PS:
        if (pformat) *pformat = format;
        return ERROR_INT("PostScript reading is not supported\n", __func__, 1);

    case IFF_LPDF:
        if (pformat) *pformat = format;
        return ERROR_INT("Pdf reading is not supported\n", __func__, 1);

    case IFF_SPIX:
        ret = sreadHeaderSpix((l_uint32 *)data, size, &w, &h, &bps,
                               &spp, &iscmap);
        if (ret)
            return ERROR_INT( "pnm: no header info returned", __func__, 1);
        break;

    case IFF_UNKNOWN:
        return ERROR_INT("unknown format; no data returned", __func__, 1);
    }

    if (pw) *pw = w;
    if (ph) *ph = h;
    if (pbps) *pbps = bps;
    if (pspp) *pspp = spp;
    if (piscmap) *piscmap = iscmap;
    if (pformat) *pformat = format;
    return 0;
}


/*---------------------------------------------------------------------*
 *                    Output image file information                    *
 *---------------------------------------------------------------------*/
extern const char *ImageFileFormatExtensions[];

/*!
 * \brief   writeImageFileInfo()
 *
 * \param[in]    filename    input file
 * \param[in]    fpout       output file stream
 * \param[in]    headeronly  1 to read only the header; 0 to read both
 *                           the header and the input file
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If headeronly == 0 and the image has spp == 4,this will
 *          also call pixDisplayLayersRGBA() to display the image
 *          in three views.
 *      (2) This is a debug function that changes the value of
 *          var_PNG_STRIP_16_TO_8 to 1 (the default).
 * </pre>
 */
l_ok
writeImageFileInfo(const char  *filename,
                   FILE        *fpout,
                   l_int32      headeronly)
{
char     *text;
l_int32   w, h, d, wpl, count, npages, color;
l_int32   format, bps, spp, iscmap, xres, yres, transparency;
FILE     *fpin;
PIX      *pix, *pixt;
PIXCMAP  *cmap;

    if (!filename)
        return ERROR_INT("filename not defined", __func__, 1);
    if (!fpout)
        return ERROR_INT("stream not defined", __func__, 1);

        /* Read the header */
    if (pixReadHeader(filename, &format, &w, &h, &bps, &spp, &iscmap)) {
        L_ERROR("failure to read header of %s\n", __func__, filename);
        return 1;
    }
    fprintf(fpout, "===============================================\n"
                    "Reading the header:\n");
    fprintf(fpout, "  input image format type: %s\n",
            ImageFileFormatExtensions[format]);
    fprintf(fpout, "  w = %d, h = %d, bps = %d, spp = %d, iscmap = %d\n",
            w, h, bps, spp, iscmap);

    findFileFormat(filename, &format);
    if (format == IFF_JP2) {
        fpin = fopenReadStream(filename);
        fgetJp2kResolution(fpin, &xres, &yres);
        if (fpin) fclose(fpin);
        fprintf(fpout, "  xres = %d, yres = %d\n", xres, yres);
    } else if (format == IFF_PNG) {
        fpin = fopenReadStream(filename);
        fgetPngResolution(fpin, &xres, &yres);
        if (fpin) fclose(fpin);
        fprintf(fpout, "  xres = %d, yres = %d\n", xres, yres);
        if (iscmap) {
            fpin = fopenReadStream(filename);
            fgetPngColormapInfo(fpin, &cmap, &transparency);
            if (fpin) fclose(fpin);
            if (transparency)
                fprintf(fpout, "  colormap has transparency\n");
            else
                fprintf(fpout, "  colormap does not have transparency\n");
            pixcmapWriteStream(fpout, cmap);
            pixcmapDestroy(&cmap);
        }
    } else if (format == IFF_JFIF_JPEG) {
        fpin = fopenReadStream(filename);
        fgetJpegResolution(fpin, &xres, &yres);
        if (fpin) fclose(fpin);
        fprintf(fpout, "  xres = %d, yres = %d\n", xres, yres);
    }

    if (headeronly)
        return 0;

        /* Read the full image.  Note that when we read an image that
         * has transparency in a colormap, we convert it to RGBA. */
    fprintf(fpout, "===============================================\n"
                    "Reading the full image:\n");

        /* Preserve 16 bpp if the format is png */
    if (format == IFF_PNG && bps == 16)
        l_pngSetReadStrip16To8(0);

    if ((pix = pixRead(filename)) == NULL) {
        L_ERROR("failure to read full image of %s\n", __func__, filename);
        return 1;
    }

    format = pixGetInputFormat(pix);
    pixGetDimensions(pix, &w, &h, &d);
    wpl = pixGetWpl(pix);
    spp = pixGetSpp(pix);
    fprintf(fpout, "  input image format type: %s\n",
            ImageFileFormatExtensions[format]);
    fprintf(fpout, "  w = %d, h = %d, d = %d, spp = %d, wpl = %d\n",
            w, h, d, spp, wpl);
    fprintf(fpout, "  xres = %d, yres = %d\n",
            pixGetXRes(pix), pixGetYRes(pix));

    text = pixGetText(pix);
    if (text)  /*  not null */
        fprintf(fpout, "  text: %s\n", text);

    cmap = pixGetColormap(pix);
    if (cmap) {
        pixcmapHasColor(cmap, &color);
        if (color)
            fprintf(fpout, "  colormap exists and has color values:");
        else
            fprintf(fpout, "  colormap exists and has only gray values:");
        pixcmapWriteStream(fpout, pixGetColormap(pix));
    }
    else
        fprintf(fpout, "  colormap does not exist\n");

    if (format == IFF_TIFF || format == IFF_TIFF_G4 ||
        format == IFF_TIFF_G3 || format == IFF_TIFF_PACKBITS) {
        fprintf(fpout, "  Tiff header information:\n");
        fpin = fopenReadStream(filename);
        tiffGetCount(fpin, &npages);
        if (fpin) fclose(fpin);
        if (npages == 1)
            fprintf(fpout, "    One page in file\n");
        else
            fprintf(fpout, "    %d pages in file\n", npages);
        fprintTiffInfo(fpout, filename);
    }

    if (d == 1) {
        pixCountPixels(pix, &count, NULL);
        pixGetDimensions(pix, &w, &h, NULL);
        fprintf(fpout, "  1 bpp: foreground pixel fraction ON/Total = %g\n",
                (l_float32)count / (l_float32)(w * h));
    }
    fprintf(fpout, "===============================================\n");

        /* If there is an alpha component, visualize it.  Note that when
         * alpha == 0, the rgb layer is transparent.  We visualize the
         * result when a white background is visible through the
         * transparency layer. */
    if (pixGetSpp(pix) == 4) {
        pixt = pixDisplayLayersRGBA(pix, 0xffffff00, 600);
        pixDisplay(pixt, 100, 100);
        pixDestroy(&pixt);
    }

    if (format == IFF_PNG && bps == 16)
        l_pngSetReadStrip16To8(1);  /* return to default if format is png */

    pixDestroy(&pix);
    return 0;
}


/*---------------------------------------------------------------------*
 *             Test function for I/O with different formats            *
 *---------------------------------------------------------------------*/
/*!
 * \brief   ioFormatTest()
 *
 * \param[in]    filename    input image file
 * \return  0 if OK; 1 on error or if the test fails
 *
 * <pre>
 * Notes:
 *      (1) This writes and reads a set of output files losslessly
 *          in different formats to /tmp/format/, and tests that the
 *          result before and after is unchanged.
 *      (2) This should work properly on input images of any depth,
 *          with and without colormaps.
 *      (3) All supported formats are tested for bmp, png, tiff and
 *          non-ascii pnm.  Ascii pnm also works (but who'd ever want
 *          to use it?)   We allow 2 bpp bmp, although it's not
 *          supported elsewhere.  And we don't support reading
 *          16 bpp png, although this can be turned on in pngio.c.
 *      (4) This silently skips png or tiff testing if HAVE_LIBPNG
 *          or HAVE_LIBTIFF are 0, respectively.
 * </pre>
 */
l_ok
ioFormatTest(const char  *filename)
{
l_int32    w, h, d, equal, problems;
#if HAVE_LIBJPEG || HAVE_LIBWEBP || HAVE_LIBJP2K
l_int32    depth;
#endif
#if HAVE_LIBJPEG || HAVE_LIBTIFF || HAVE_LIBWEBP || HAVE_LIBJP2K
l_float32  diff;
#endif
BOX       *box;
PIX       *pixs, *pixc, *pix1, *pix2;
PIXCMAP   *cmap;

    if (!filename)
        return ERROR_INT("filename not defined", __func__, 1);

        /* Read the input file and limit the size */
    if ((pix1 = pixRead(filename)) == NULL)
        return ERROR_INT("pix1 not made", __func__, 1);
    pixGetDimensions(pix1, &w, &h, NULL);
    if (w > 250 && h > 250) {  /* take the central 250 x 250 region */
        box = boxCreate(w / 2 - 125, h / 2 - 125, 250, 250);
        pixs = pixClipRectangle(pix1, box, NULL);
        boxDestroy(&box);
    } else {
        pixs = pixClone(pix1);
    }
    pixDestroy(&pix1);

    lept_mkdir("lept/format");

        /* Note that the reader automatically removes colormaps
         * from 1 bpp BMP images, but not from 8 bpp BMP images.
         * Therefore, if our 8 bpp image initially doesn't have a
         * colormap, we are going to need to remove it from any
         * pix read from a BMP file. */
    pixc = pixClone(pixs);  /* laziness */

        /* This does not test the alpha layer pixels, because most
         * formats don't support it.  Remove any alpha.  */
    if (pixGetSpp(pixc) == 4)
        pixSetSpp(pixc, 3);
    cmap = pixGetColormap(pixc);  /* colormap; can be NULL */
    d = pixGetDepth(pixc);

    problems = FALSE;

        /* ----------------------- BMP -------------------------- */

        /* BMP works for 1, 2, 4, 8 and 32 bpp images.
         * It always writes colormaps for 1 and 8 bpp, so we must
         * remove it after readback if the input image doesn't have
         * a colormap.  Although we can write/read 2 bpp BMP, nobody
         * else can read them! */
    if (d == 1 || d == 8) {
        L_INFO("write/read bmp\n", __func__);
        pixWrite(FILE_BMP, pixc, IFF_BMP);
        pix1 = pixRead(FILE_BMP);
        if (!cmap)
            pix2 = pixRemoveColormap(pix1, REMOVE_CMAP_BASED_ON_SRC);
        else
            pix2 = pixClone(pix1);
        pixEqual(pixc, pix2, &equal);
        if (!equal) {
            L_INFO("   **** bad bmp image: d = %d ****\n", __func__, d);
            problems = TRUE;
        }
        pixDestroy(&pix1);
        pixDestroy(&pix2);
    }

    if (d == 2 || d == 4 || d == 32) {
        L_INFO("write/read bmp\n", __func__);
        pixWrite(FILE_BMP, pixc, IFF_BMP);
        pix1 = pixRead(FILE_BMP);
        pixEqual(pixc, pix1, &equal);
        if (!equal) {
            L_INFO("   **** bad bmp image: d = %d ****\n", __func__, d);
            problems = TRUE;
        }
        pixDestroy(&pix1);
    }

        /* ----------------------- PNG -------------------------- */
#if HAVE_LIBPNG
        /* PNG works for all depths, but here, because we strip
         * 16 --> 8 bpp on reading, we don't test png for 16 bpp. */
    if (d != 16) {
        L_INFO("write/read png\n", __func__);
        pixWrite(FILE_PNG, pixc, IFF_PNG);
        pix1 = pixRead(FILE_PNG);
        pixEqual(pixc, pix1, &equal);
        if (!equal) {
            L_INFO("   **** bad png image: d = %d ****\n", __func__, d);
            problems = TRUE;
        }
        pixDestroy(&pix1);
    }
#endif  /* HAVE_LIBPNG */

        /* ----------------------- TIFF -------------------------- */
#if HAVE_LIBTIFF && HAVE_LIBJPEG
        /* TIFF works for 1, 2, 4, 8, 16 and 32 bpp images.
         * Because 8 bpp tiff always writes 256 entry colormaps, the
         * colormap sizes may be different for 8 bpp images with
         * colormap; we are testing if the image content is the same.
         * Likewise, the 2 and 4 bpp tiff images with colormaps
         * have colormap sizes 4 and 16, rsp.  This test should
         * work properly on the content, regardless of the number
         * of color entries in pixc. */

        /* tiff uncompressed works for all pixel depths */
    L_INFO("write/read uncompressed tiff\n", __func__);
    pixWrite(FILE_TIFF, pixc, IFF_TIFF);
    pix1 = pixRead(FILE_TIFF);
    pixEqual(pixc, pix1, &equal);
    if (!equal) {
        L_INFO("   **** bad tiff uncompressed image: d = %d ****\n",
               __func__, d);
        problems = TRUE;
    }
    pixDestroy(&pix1);

        /* tiff lzw works for all pixel depths */
    L_INFO("write/read lzw compressed tiff\n", __func__);
    pixWrite(FILE_LZW, pixc, IFF_TIFF_LZW);
    pix1 = pixRead(FILE_LZW);
    pixEqual(pixc, pix1, &equal);
    if (!equal) {
        L_INFO("   **** bad tiff lzw compressed image: d = %d ****\n",
               __func__, d);
        problems = TRUE;
    }
    pixDestroy(&pix1);

        /* tiff adobe deflate (zip) works for all pixel depths */
    L_INFO("write/read zip compressed tiff\n", __func__);
    pixWrite(FILE_ZIP, pixc, IFF_TIFF_ZIP);
    pix1 = pixRead(FILE_ZIP);
    pixEqual(pixc, pix1, &equal);
    if (!equal) {
        L_INFO("   **** bad tiff zip compressed image: d = %d ****\n",
               __func__, d);
        problems = TRUE;
    }
    pixDestroy(&pix1);

        /* tiff jpeg encoding works for grayscale and rgb */
    if (d == 8 || d == 32) {
        PIX  *pixc1;
        L_INFO("write/read jpeg compressed tiff\n", __func__);
        if (d == 8 && pixGetColormap(pixc)) {
            pixc1 = pixRemoveColormap(pixc, REMOVE_CMAP_BASED_ON_SRC);
            pixWrite(FILE_TIFF_JPEG, pixc1, IFF_TIFF_JPEG);
            if ((pix1 = pixRead(FILE_TIFF_JPEG)) == NULL) {
                L_INFO(" did not read FILE_TIFF_JPEG\n", __func__);
                problems = TRUE;
            }
            pixDestroy(&pixc1);
        } else {
            pixWrite(FILE_TIFF_JPEG, pixc, IFF_TIFF_JPEG);
            pix1 = pixRead(FILE_TIFF_JPEG);
            if (d == 8) {
                pixCompareGray(pix1, pixc, L_COMPARE_ABS_DIFF, 0, NULL, &diff,
                               NULL, NULL);
            } else {
                pixCompareRGB(pix1, pixc, L_COMPARE_ABS_DIFF, 0, NULL, &diff,
                              NULL, NULL);
            }
            if (diff > 8.0) {
                L_INFO("   **** bad tiff jpeg compressed image: "
                       "d = %d, diff = %5.2f ****\n", __func__, d, diff);
                problems = TRUE;
            }
        }
        pixDestroy(&pix1);
    }

        /* tiff g4, g3, rle and packbits work for 1 bpp */
    if (d == 1) {
        L_INFO("write/read g4 compressed tiff\n", __func__);
        pixWrite(FILE_G4, pixc, IFF_TIFF_G4);
        pix1 = pixRead(FILE_G4);
        pixEqual(pixc, pix1, &equal);
        if (!equal) {
            L_INFO("   **** bad tiff g4 image ****\n", __func__);
            problems = TRUE;
        }
        pixDestroy(&pix1);

        L_INFO("write/read g3 compressed tiff\n", __func__);
        pixWrite(FILE_G3, pixc, IFF_TIFF_G3);
        pix1 = pixRead(FILE_G3);
        pixEqual(pixc, pix1, &equal);
        if (!equal) {
            L_INFO("   **** bad tiff g3 image ****\n", __func__);
            problems = TRUE;
        }
        pixDestroy(&pix1);

        L_INFO("write/read rle compressed tiff\n", __func__);
        pixWrite(FILE_RLE, pixc, IFF_TIFF_RLE);
        pix1 = pixRead(FILE_RLE);
        pixEqual(pixc, pix1, &equal);
        if (!equal) {
            L_INFO("   **** bad tiff rle image: d = %d ****\n", __func__, d);
            problems = TRUE;
        }
        pixDestroy(&pix1);

        L_INFO("write/read packbits compressed tiff\n", __func__);
        pixWrite(FILE_PB, pixc, IFF_TIFF_PACKBITS);
        pix1 = pixRead(FILE_PB);
        pixEqual(pixc, pix1, &equal);
        if (!equal) {
            L_INFO("   **** bad tiff packbits image: d = %d ****\n",
                   __func__, d);
            problems = TRUE;
        }
        pixDestroy(&pix1);
    }
#endif  /* HAVE_LIBTIFF && HAVE_LIBJPEG */

        /* ----------------------- PNM -------------------------- */

        /* pnm works for 1, 2, 4, 8, 16 and 32 bpp.
         * pnm doesn't have colormaps, so when we write colormapped
         * pix out as pnm, the colormap is removed.  Thus for the test,
         * we must remove the colormap from pixc before testing.  */
    L_INFO("write/read pnm\n", __func__);
    pixWrite(FILE_PNM, pixc, IFF_PNM);
    pix1 = pixRead(FILE_PNM);
    if (cmap)
        pix2 = pixRemoveColormap(pixc, REMOVE_CMAP_BASED_ON_SRC);
    else
        pix2 = pixClone(pixc);
    pixEqual(pix1, pix2, &equal);
    if (!equal) {
        L_INFO("   **** bad pnm image: d = %d ****\n", __func__, d);
        problems = TRUE;
    }
    pixDestroy(&pix1);
    pixDestroy(&pix2);

        /* ----------------------- GIF -------------------------- */
#if HAVE_LIBGIF
        /* GIF works for only 1 and 8 bpp, colormapped */
    if (d != 8 || !cmap)
        pix1 = pixConvertTo8(pixc, 1);
    else
        pix1 = pixClone(pixc);
    L_INFO("write/read gif\n", __func__);
    pixWrite(FILE_GIF, pix1, IFF_GIF);
    pix2 = pixRead(FILE_GIF);
    pixEqual(pix1, pix2, &equal);
    if (!equal) {
        L_INFO("   **** bad gif image: d = %d ****\n", __func__, d);
        problems = TRUE;
    }
    pixDestroy(&pix1);
    pixDestroy(&pix2);
#endif  /* HAVE_LIBGIF */

        /* ----------------------- JPEG ------------------------- */
#if HAVE_LIBJPEG
        /* JPEG works for only 8 bpp gray and rgb */
    if (cmap || d > 8)
        pix1 = pixConvertTo32(pixc);
    else
        pix1 = pixConvertTo8(pixc, 0);
    depth = pixGetDepth(pix1);
    L_INFO("write/read jpeg\n", __func__);
    pixWrite(FILE_JPG, pix1, IFF_JFIF_JPEG);
    pix2 = pixRead(FILE_JPG);
    if (depth == 8) {
        pixCompareGray(pix1, pix2, L_COMPARE_ABS_DIFF, 0, NULL, &diff,
                       NULL, NULL);
    } else {
        pixCompareRGB(pix1, pix2, L_COMPARE_ABS_DIFF, 0, NULL, &diff,
                      NULL, NULL);
    }
    if (diff > 8.0) {
        L_INFO("   **** bad jpeg image: d = %d, diff = %5.2f ****\n",
               __func__, depth, diff);
        problems = TRUE;
    }
    pixDestroy(&pix1);
    pixDestroy(&pix2);
#endif  /* HAVE_LIBJPEG */

        /* ----------------------- WEBP ------------------------- */
#if HAVE_LIBWEBP
        /* WEBP works for rgb and rgba */
    if (cmap || d <= 16)
        pix1 = pixConvertTo32(pixc);
    else
        pix1 = pixClone(pixc);
    depth = pixGetDepth(pix1);
    L_INFO("write/read webp\n", __func__);
    pixWrite(FILE_WEBP, pix1, IFF_WEBP);
    pix2 = pixRead(FILE_WEBP);
    pixCompareRGB(pix1, pix2, L_COMPARE_ABS_DIFF, 0, NULL, &diff, NULL, NULL);
    if (diff > 5.0) {
        L_INFO("   **** bad webp image: d = %d, diff = %5.2f ****\n",
               __func__, depth, diff);
        problems = TRUE;
    }
    pixDestroy(&pix1);
    pixDestroy(&pix2);
#endif  /* HAVE_LIBWEBP */

        /* ----------------------- JP2K ------------------------- */
#if HAVE_LIBJP2K
        /* JP2K works for only 8 bpp gray, rgb and rgba */
    if (cmap || d > 8)
        pix1 = pixConvertTo32(pixc);
    else
        pix1 = pixConvertTo8(pixc, 0);
    depth = pixGetDepth(pix1);
    L_INFO("write/read jp2k\n", __func__);
    pixWrite(FILE_JP2K, pix1, IFF_JP2);
    pix2 = pixRead(FILE_JP2K);
    if (depth == 8) {
        pixCompareGray(pix1, pix2, L_COMPARE_ABS_DIFF, 0, NULL, &diff,
                       NULL, NULL);
    } else {
        pixCompareRGB(pix1, pix2, L_COMPARE_ABS_DIFF, 0, NULL, &diff,
                      NULL, NULL);
    }
    lept_stderr("diff = %7.3f\n", diff);
    if (diff > 7.0) {
        L_INFO("   **** bad jp2k image: d = %d, diff = %5.2f ****\n",
               __func__, depth, diff);
        problems = TRUE;
    }
    pixDestroy(&pix1);
    pixDestroy(&pix2);
#endif  /* HAVE_LIBJP2K */

    if (problems == FALSE)
        L_INFO("All formats read and written OK!\n", __func__);

    pixDestroy(&pixc);
    pixDestroy(&pixs);
    return problems;
}
