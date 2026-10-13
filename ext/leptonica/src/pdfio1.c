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
 * \file pdfio1.c
 * <pre>
 *
 *    Higher-level operations for generating pdf from images.
 *    Use poppler's pdftoppm or pdfimages to invert the process,
 *    extracting raster images from pdf.
 *
 *    |=============================================================|
 *    |                        Important notes                      |
 *    |=============================================================|
 *    | Some of these functions require I/O libraries such as       |
 *    | libtiff, libjpeg, libpng, libz and libopenjp2.  If you do   |
 *    | not have these libraries, some calls will fail.  For        |
 *    | example, if you do not have libopenjp2, you cannot write a  |
 *    | pdf where transcoding is required to incorporate a          |
 *    | jp2k image.                                                 |
 *    |                                                             |
 *    | You can manually deactivate all pdf writing by setting      |
 *    | this in environ.h:                                          |
 *    | \code                                                       |
 *    |      #define  USE_PDFIO     0                               |
 *    | \endcode                                                    |
 *    | This will link the stub file pdfiostub.c.                   |
 *    |=============================================================|
 *
 *     Set 1. These functions convert a set of image files
 *     to a multi-page pdf file, with one image on each page.
 *     All images are rendered at the same (input) resolution.
 *     The images can be specified as being in a directory, or they
 *     can be in an sarray.  The output pdf can be either a file
 *     or an array of bytes in memory.
 *
 *     Set 2. These functions are a special case of set 1, where
 *     no scaling or change in quality is required.  For jpeg, jp2k and
 *     tiffg4 images, the bytes in each file can be directly incorporated
 *     into the output pdf, and the wrapping up of multiple image
 *     files is very fast.  For non-interlaced png, the data bytes
 *     including the predictors can also be written directly into the
 *     flate pdf data.  For other image formats transcoding is required,
 *     where the image data is first decompressed and then flate (gzip),
 *     DCT (jpeg) or tiffg4 (1 bpp) encodings are generated.
 *
 *     Set 3. These functions convert a set of images in memory
 *     to a multi-page pdf, with one image on each page.  The pdf
 *     output can be either a file or an array of bytes in memory.
 *
 *     Set 4. These functions implement a pdf output "device driver"
 *     for wrapping (encoding) any number of images on a single page
 *     in pdf.  The input can be either an image file or a Pix;
 *     the pdf output can be either a file or an array of bytes in memory.
 *
 *     Set 5. These "segmented" functions take a set of image
 *     files, along with optional segmentation information, and
 *     generate a multi-page pdf file, where each page consists
 *     in general of a mixed raster pdf of image and non-image regions.
 *     The segmentation information for each page can be input as
 *     either a mask over the image parts, or as a Boxa of those
 *     regions.
 *
 *     Set 6. These "segmented" functions convert an image and
 *     an optional Boxa of image regions into a mixed raster pdf file
 *     for the page.  The input image can be either a file or a Pix.
 *
 *     Set 7. These functions take a set of single-page pdf files
 *     and concatenates it into a multi-page pdf.  The input can be
 *     a set of either single page pdf files or pdf 'strings' in memory.
 *     The output can be either a file or an array of bytes in memory.
 *
 *     The images in the pdf file can be rendered using a pdf viewer,
 *     such as evince, gv, xpdf or acroread.
 *
 *     Reference on the pdf file format:
 *         http://www.adobe.com/devnet/pdf/pdf_reference_archive.html
 *
 *     1. Convert specified image files to pdf (one image file per page)
 *          l_int32             convertFilesToPdf()
 *          l_int32             saConvertFilesToPdf()
 *          l_int32             saConvertFilesToPdfData()
 *          l_int32             selectDefaultPdfEncoding()
 *
 *     2. Convert specified image files to pdf without scaling
 *          l_int32             convertUnscaledFilesToPdf()
 *          l_int32             saConvertUnscaledFilesToPdf()
 *          l_int32             saConvertUnscaledFilesToPdfData()
 *          l_int32             convertUnscaledToPdfData()
 *
 *     3. Convert multiple images to pdf (one image per page)
 *          l_int32             pixaConvertToPdf()
 *          l_int32             pixaConvertToPdfData()
 *
 *     4. Single page, multi-image converters
 *          l_int32             convertToPdf()
 *          l_int32             convertImageDataToPdf()
 *          l_int32             convertToPdfData()
 *          l_int32             convertImageDataToPdfData()
 *          l_int32             pixConvertToPdf()
 *          l_int32             pixWriteStreamPdf()
 *          l_int32             pixWriteMemPdf()
 *
 *     5. Segmented multi-page, multi-image converter
 *          l_int32             convertSegmentedFilesToPdf()
 *          BOXAA              *convertNumberedMasksToBoxaa()
 *
 *     6. Segmented single page, multi-image converters
 *          l_int32             convertToPdfSegmented()
 *          l_int32             pixConvertToPdfSegmented()
 *          l_int32             convertToPdfDataSegmented()
 *          l_int32             pixConvertToPdfDataSegmented()
 *
 *     7. Multipage concatenation
 *          l_int32             concatenatePdf()
 *          l_int32             saConcatenatePdf()
 *          l_int32             ptraConcatenatePdf()
 *          l_int32             concatenatePdfToData()
 *          l_int32             saConcatenatePdfToData()
 *
 *     The top-level multi-image functions can be visualized as follows:
 *          Output pdf data to file:
 *             convertToPdf()  and  convertImageDataToPdf()
 *                     --> pixConvertToPdf()
 *                           --> pixConvertToPdfData()
 *
 *          Output pdf data to array in memory:
 *             convertToPdfData()  and  convertImageDataToPdfData()
 *                     --> pixConvertToPdfData()
 *
 *     The top-level segmented image functions can be visualized as follows:
 *          Output pdf data to file:
 *             convertToPdfSegmented()
 *                     --> pixConvertToPdfSegmented()
 *                           --> pixConvertToPdfDataSegmented()
 *
 *          Output pdf data to array in memory:
 *             convertToPdfDataSegmented()
 *                     --> pixConvertToPdfDataSegmented()
 *
 *     For multi-page concatenation, there are three different types of input
 *        (1) directory and optional filename filter
 *        (2) sarray of filenames
 *        (3) ptra of byte arrays of pdf data
 *     and two types of output for the concatenated pdf data
 *        (1) filename
 *        (2) data array and size
 *     High-level interfaces are given for each of the six combinations.
 *
 *     Note: When wrapping small images into pdf, it is useful to give
 *     them a relatively low resolution value, to avoid rounding errors
 *     when rendering the images.  For example, if you want an image
 *     of width w pixels to be 5 inches wide on a screen, choose a
 *     resolution w/5.
 *
 *     The very fast functions in section (2) require neither transcoding
 *     nor parsing of the compressed jpeg file.  With three types of image
 *     compression, the compressed strings can be incorporated into
 *     the pdf data without decompression and re-encoding: jpeg, jp2k
 *     and png.  The DCTDecode and JPXDecode filters can handle the
 *     entire jpeg and jp2k encoded string as a byte array in the pdf file.
 *     The FlateDecode filter can handle the png compressed image data,
 *     including predictors that occur as the first byte in each
 *     raster line, but it is necessary to store only the png IDAT chunk
 *     data in the pdf array.  The alternative for wrapping png images
 *     is to transcode them: uncompress into a raster (a pix) and then
 *     gzip the raster data.  This typically results in a larger pdf file
 *     because it doesn't use the two-dimensional png predictor.
 *     Colormaps, which are found in png PLTE chunks, must always be
 *     pulled out and included separately in the pdf.  For CCITT-G4
 *     compression, you can not simply include a tiff G4 file -- you must
 *     either parse it and extract the G4 compressed data within it,
 *     or uncompress to a raster and G4 compress again.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include <math.h>
#include "allheaders.h"

/* --------------------------------------------*/
#if  USE_PDFIO   /* defined in environ.h */
 /* --------------------------------------------*/

    /* Typical scan resolution in ppi (pixels/inch) */
static const l_int32  DefaultInputRes = 300;

/*---------------------------------------------------------------------*
 *    Convert specified image files to pdf (one image file per page)   *
 *---------------------------------------------------------------------*/
/*!
 * \brief   convertFilesToPdf()
 *
 * \param[in]    dirname       directory name containing images
 * \param[in]    substr        [optional] substring filter on filenames;
 *                             can be null
 * \param[in]    res           input resolution of all images
 * \param[in]    scalefactor   scaling factor applied to each image; > 0.0
 * \param[in]    type          encoding type (L_JPEG_ENCODE, L_G4_ENCODE,
 *                             L_FLATE_ENCODE, L_JP2K_ENCODE or
 *                             L_DEFAULT_ENCODE for default)
 * \param[in]    quality       for jpeg: 1-100; 0 for default (75)
 *                             for jp2k: 27-45; 0 for default (34)
 * \param[in]    title         [optional] pdf title; can be null
 * \param[in]    fileout       pdf file of all images
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If %substr is not NULL, only image filenames that contain
 *          the substring can be used.  If %substr == NULL, all files
 *          in the directory are used.
 *      (2) The files in the directory, after optional filtering by
 *          the substring, are lexically sorted in increasing order
 *          before concatenation.
 *      (3) The scalefactor is applied to each image before encoding.
 *          If you enter a value <= 0.0, it will be set to 1.0.
 *      (4) Specifying one of the four encoding types for %type forces
 *          all images to be compressed with that type.  Use 0 to have
 *          the type determined for each image based on depth and whether
 *          or not it has a colormap.
 * </pre>
 */
l_ok
convertFilesToPdf(const char  *dirname,
                  const char  *substr,
                  l_int32      res,
                  l_float32    scalefactor,
                  l_int32      type,
                  l_int32      quality,
                  const char  *title,
                  const char  *fileout)
{
l_int32  ret;
SARRAY  *sa;

    if (!dirname)
        return ERROR_INT("dirname not defined", __func__, 1);
    if (!fileout)
        return ERROR_INT("fileout not defined", __func__, 1);

    if ((sa = getSortedPathnamesInDirectory(dirname, substr, 0, 0)) == NULL)
        return ERROR_INT("sa not made", __func__, 1);
    ret = saConvertFilesToPdf(sa, res, scalefactor, type, quality,
                              title, fileout);
    sarrayDestroy(&sa);
    return ret;
}


/*!
 * \brief   saConvertFilesToPdf()
 *
 * \param[in]    sa            string array of pathnames for images
 * \param[in]    res           input resolution of all images
 * \param[in]    scalefactor   scaling factor applied to each image; > 0.0
 * \param[in]    type          encoding type (L_JPEG_ENCODE, L_G4_ENCODE,
 *                             L_FLATE_ENCODE, L_JP2K_ENCODE or
 *                             L_DEFAULT_ENCODE for default)
 * \param[in]    quality       for jpeg: 1-100; 0 for default (75)
 *                             for jp2k: 27-45; 0 for default (34)
 * \param[in]    title         [optional] pdf title; can be null
 * \param[in]    fileout       pdf file of all images
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) See convertFilesToPdf().
 * </pre>
 */
l_ok
saConvertFilesToPdf(SARRAY      *sa,
                    l_int32      res,
                    l_float32    scalefactor,
                    l_int32      type,
                    l_int32      quality,
                    const char  *title,
                    const char  *fileout)
{
l_uint8  *data;
l_int32   ret;
size_t    nbytes;

    if (!sa)
        return ERROR_INT("sa not defined", __func__, 1);

    ret = saConvertFilesToPdfData(sa, res, scalefactor, type, quality,
                                  title, &data, &nbytes);
    if (ret) {
        if (data) LEPT_FREE(data);
        return ERROR_INT("pdf data not made", __func__, 1);
    }

    ret = l_binaryWrite(fileout, "w", data, nbytes);
    LEPT_FREE(data);
    if (ret)
        L_ERROR("pdf data not written to file\n", __func__);
    return ret;
}


/*!
 * \brief   saConvertFilesToPdfData()
 *
 * \param[in]    sa            string array of pathnames for images
 * \param[in]    res           input resolution of all images
 * \param[in]    scalefactor   scaling factor applied to each image; > 0.0
 * \param[in]    type          encoding type (L_JPEG_ENCODE, L_G4_ENCODE,
 *                             L_FLATE_ENCODE, L_JP2K_ENCODE or
 *                             L_DEFAULT_ENCODE for default)
 * \param[in]    quality       for jpeg: 1-100; 0 for default (75)
 *                             for jp2k: 27-45; 0 for default (34)
 * \param[in]    title         [optional] pdf title; can be null
 * \param[out]   pdata         output pdf data (of all images
 * \param[out]   pnbytes       size of output pdf data
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) See convertFilesToPdf().
 * </pre>
 */
l_ok
saConvertFilesToPdfData(SARRAY      *sa,
                        l_int32      res,
                        l_float32    scalefactor,
                        l_int32      type,
                        l_int32      quality,
                        const char  *title,
                        l_uint8    **pdata,
                        size_t      *pnbytes)
{
char     *fname;
l_uint8  *imdata;
l_int32   i, n, ret, pagetype, npages, scaledres;
size_t    imbytes;
L_BYTEA  *ba;
PIX      *pixs, *pix;
L_PTRA   *pa_data;

    if (!pdata)
        return ERROR_INT("&data not defined", __func__, 1);
    *pdata = NULL;
    if (!pnbytes)
        return ERROR_INT("&nbytes not defined", __func__, 1);
    *pnbytes = 0;
    if (!sa)
        return ERROR_INT("sa not defined", __func__, 1);
    if (scalefactor <= 0.0) scalefactor = 1.0;
    if (type != L_JPEG_ENCODE && type != L_G4_ENCODE &&
        type != L_FLATE_ENCODE && type != L_JP2K_ENCODE) {
        type = L_DEFAULT_ENCODE;
    }

        /* Generate all the encoded pdf strings */
    n = sarrayGetCount(sa);
    pa_data = ptraCreate(n);
    for (i = 0; i < n; i++) {
        if (i && (i % 10 == 0)) lept_stderr(".. %d ", i);
        fname = sarrayGetString(sa, i, L_NOCOPY);
        if ((pixs = pixRead(fname)) == NULL) {
            L_ERROR("image not readable from file %s\n", __func__, fname);
            continue;
        }
        if (scalefactor != 1.0)
            pix = pixScale(pixs, scalefactor, scalefactor);
        else
            pix = pixClone(pixs);
        pixDestroy(&pixs);
        scaledres = (l_int32)(res * scalefactor);

            /* Select the encoding type */
        if (type != L_DEFAULT_ENCODE) {
            pagetype = type;
        } else if (selectDefaultPdfEncoding(pix, &pagetype) != 0) {
            pixDestroy(&pix);
            L_ERROR("encoding type selection failed for file %s\n",
                    __func__, fname);
            continue;
        }

        ret = pixConvertToPdfData(pix, pagetype, quality, &imdata, &imbytes,
                                  0, 0, scaledres, title, NULL, 0);
        pixDestroy(&pix);
        if (ret) {
            LEPT_FREE(imdata);
            L_ERROR("pdf encoding failed for %s\n", __func__, fname);
            continue;
        }
        ba = l_byteaInitFromMem(imdata, imbytes);
        LEPT_FREE(imdata);
        ptraAdd(pa_data, ba);
    }
    ptraGetActualCount(pa_data, &npages);
    if (npages == 0) {
        L_ERROR("no pdf files made\n", __func__);
        ptraDestroy(&pa_data, FALSE, FALSE);
        return 1;
    }

        /* Concatenate them */
    lept_stderr("\nconcatenating ... ");
    ret = ptraConcatenatePdfToData(pa_data, NULL, pdata, pnbytes);
    lept_stderr("done\n");

    ptraGetActualCount(pa_data, &npages);  /* recalculate in case it changes */
    for (i = 0; i < npages; i++) {
        ba = (L_BYTEA *)ptraRemove(pa_data, i, L_NO_COMPACTION);
        l_byteaDestroy(&ba);
    }
    ptraDestroy(&pa_data, FALSE, FALSE);
    return ret;
}


/*!
 * \brief   selectDefaultPdfEncoding()
 *
 * \param[in]    pix
 * \param[out]   ptype     L_G4_ENCODE, L_JPEG_ENCODE, L_FLATE_ENCODE
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This attempts to choose an encoding for the pix that results
 *          in the smallest file, assuming that if jpeg encoded, it will
 *          use quality = 75.  The decision is approximate, in that
 *          (a) all colormapped images will be losslessly encoded with
 *          gzip (flate), and (b) an image with less than about 20 colors
 *          is likely to be smaller if flate encoded than if encoded
 *          as a jpeg (dct).  For example, an image made by pixScaleToGray3()
 *          will have 10 colors, and flate encoding will give about
 *          twice the compression as jpeg with quality = 75.
 *      (2) We could have used L_JP2K_ENCODE instead of L_JPEG_ENCODE.
 *          However, the jp2k compression is not much better than jpeg, and
 *          the jpeg library is more commonly available than the jp2k library.
 * </pre>
 */
l_ok
selectDefaultPdfEncoding(PIX      *pix,
                         l_int32  *ptype)
{
l_int32   w, h, d, factor, ncolors;
PIXCMAP  *cmap;

    if (!ptype)
        return ERROR_INT("&type not defined", __func__, 1);
    *ptype = L_FLATE_ENCODE;  /* default universal encoding */
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    pixGetDimensions(pix, &w, &h, &d);
    cmap = pixGetColormap(pix);
    if (d == 8 && !cmap) {
        factor = L_MAX(1, (l_int32)sqrt((l_float64)(w * h) / 20000.));
        pixNumColors(pix, factor, &ncolors);
        if (ncolors < 20)
            *ptype = L_FLATE_ENCODE;
        else
            *ptype = L_JPEG_ENCODE;
    } else if (d == 1) {
        *ptype = L_G4_ENCODE;
    } else if (cmap || d == 2 || d == 4) {
        *ptype = L_FLATE_ENCODE;
    } else if (d == 8 || d == 32) {
        *ptype = L_JPEG_ENCODE;
    } else if (d == 16) {
        *ptype = L_FLATE_ENCODE;
    } else {
        return ERROR_INT("type selection failure", __func__, 1);
    }

    return 0;
}


/*---------------------------------------------------------------------*
 *          Convert specified image files to pdf without scaling       *
 *---------------------------------------------------------------------*/
/*!
 * \brief   convertUnscaledFilesToPdf()
 *
 * \param[in]    dirname   directory name containing images
 * \param[in]    substr    [optional] substring filter on filenames;
 *                         can be null
 * \param[in]    title     [optional] pdf title; can be null
 * \param[in]    fileout   pdf file of all images
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If %substr is not NULL, only image filenames that contain
 *          the substring can be used.  If %substr == NULL, all files
 *          in the directory are used.
 *      (2) The files in the directory, after optional filtering by
 *          the substring, are lexically sorted in increasing order
 *          before concatenation.
 *      (3) This is very fast for jpeg, jp2k and some png files,
 *          because the compressed data is wrapped up and concatenated.
 *          For other types of png, the images must be read and recompressed.
 * </pre>
 */
l_ok
convertUnscaledFilesToPdf(const char  *dirname,
                          const char  *substr,
                          const char  *title,
                          const char  *fileout)
{
l_int32  ret;
SARRAY  *sa;

    if (!dirname)
        return ERROR_INT("dirname not defined", __func__, 1);
    if (!fileout)
        return ERROR_INT("fileout not defined", __func__, 1);

    if ((sa = getSortedPathnamesInDirectory(dirname, substr, 0, 0)) == NULL)
        return ERROR_INT("sa not made", __func__, 1);
    ret = saConvertUnscaledFilesToPdf(sa, title, fileout);
    sarrayDestroy(&sa);
    return ret;
}


/*!
 * \brief   saConvertUnscaledFilesToPdf()
 *
 * \param[in]    sa        string array of pathnames for images
 * \param[in]    title     [optional] pdf title; can be null
 * \param[in]    fileout   pdf file of all images
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) See convertUnscaledFilesToPdf().
 * </pre>
 */
l_ok
saConvertUnscaledFilesToPdf(SARRAY      *sa,
                            const char  *title,
                            const char  *fileout)
{
l_uint8  *data;
l_int32   ret;
size_t    nbytes;

    if (!sa)
        return ERROR_INT("sa not defined", __func__, 1);

    ret = saConvertUnscaledFilesToPdfData(sa, title, &data, &nbytes);
    if (ret) {
        if (data) LEPT_FREE(data);
        return ERROR_INT("pdf data not made", __func__, 1);
    }

    ret = l_binaryWrite(fileout, "w", data, nbytes);
    LEPT_FREE(data);
    if (ret)
        L_ERROR("pdf data not written to file\n", __func__);
    return ret;
}


/*!
 * \brief   saConvertUnscaledFilesToPdfData()
 *
 * \param[in]    sa        string array of pathnames for image files
 * \param[in]    title     [optional] pdf title; can be null
 * \param[out]   pdata     output pdf data (of all images)
 * \param[out]   pnbytes   size of output pdf data
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is very fast for jpeg, jp2k and some png files,
 *          because the compressed data is wrapped up and concatenated.
 *          For other types of png, the images must be read and recompressed.
 * </pre>
 */
l_ok
saConvertUnscaledFilesToPdfData(SARRAY      *sa,
                                const char  *title,
                                l_uint8    **pdata,
                                size_t      *pnbytes)
{
char         *fname;
l_uint8      *imdata;
l_int32       i, n, ret, npages;
size_t        imbytes;
L_BYTEA      *ba;
L_PTRA       *pa_data;

    if (!pdata)
        return ERROR_INT("&data not defined", __func__, 1);
    *pdata = NULL;
    if (!pnbytes)
        return ERROR_INT("&nbytes not defined", __func__, 1);
    *pnbytes = 0;
    if (!sa)
        return ERROR_INT("sa not defined", __func__, 1);

        /* Generate all the encoded pdf strings */
    n = sarrayGetCount(sa);
    pa_data = ptraCreate(n);
    for (i = 0; i < n; i++) {
        if (i && (i % 10 == 0)) lept_stderr(".. %d ", i);
        fname = sarrayGetString(sa, i, L_NOCOPY);

            /* Generate the pdf data */
        if (convertUnscaledToPdfData(fname, title, &imdata, &imbytes))
            continue;

            /* ... and add it to the array of single page data */
        ba = l_byteaInitFromMem(imdata, imbytes);
        if (imdata) LEPT_FREE(imdata);
        ptraAdd(pa_data, ba);
    }
    ptraGetActualCount(pa_data, &npages);
    if (npages == 0) {
        L_ERROR("no pdf files made\n", __func__);
        ptraDestroy(&pa_data, FALSE, FALSE);
        return 1;
    }

        /* Concatenate to generate a multipage pdf */
    lept_stderr("\nconcatenating ... ");
    ret = ptraConcatenatePdfToData(pa_data, NULL, pdata, pnbytes);
    lept_stderr("done\n");

        /* Clean up */
    ptraGetActualCount(pa_data, &npages);  /* maybe failed to read some files */
    for (i = 0; i < npages; i++) {
        ba = (L_BYTEA *)ptraRemove(pa_data, i, L_NO_COMPACTION);
        l_byteaDestroy(&ba);
    }
    ptraDestroy(&pa_data, FALSE, FALSE);
    return ret;
}


/*!
 * \brief   convertUnscaledToPdfData()
 *
 * \param[in]    fname      of image file in all formats
 * \param[in]    title      [optional] pdf title; can be null
 * \param[out]   pdata      output pdf data for image
 * \param[out]   pnbytes    size of output pdf data
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is very fast for jpeg, jp2k and some png files,
 *          because the compressed data is wrapped up and concatenated.
 *          For other types of png, the images must be read and recompressed.
 * </pre>
 */
l_ok
convertUnscaledToPdfData(const char  *fname,
                         const char  *title,
                         l_uint8    **pdata,
                         size_t      *pnbytes)
{
l_int32       format;
L_COMP_DATA  *cid;

    if (!pdata)
        return ERROR_INT("&data not defined", __func__, 1);
    *pdata = NULL;
    if (!pnbytes)
        return ERROR_INT("&nbytes not defined", __func__, 1);
    *pnbytes = 0;
    if (!fname)
        return ERROR_INT("fname not defined", __func__, 1);

    findFileFormat(fname, &format);
    if (format == IFF_UNKNOWN) {
        L_WARNING("file %s format is unknown; skip\n", __func__, fname);
        return 1;
    }
    if (format == IFF_PS || format == IFF_LPDF) {
        L_WARNING("file %s format is %d; skip\n", __func__, fname, format);
        return 1;
    }

        /* Generate the image data required for pdf generation, always
         * in binary (not ascii85) coding.  Note that jpeg, jp2k and some
         * png files are not transcoded.  */
    l_generateCIDataForPdf(fname, NULL, 0, &cid);
    if (!cid) {
        L_ERROR("file %s format is %d; unreadable\n", __func__, fname, format);
        return 1;
    }

        /* Generate the pdf string for this page (image).  This destroys
         * the cid by attaching it to an lpd and destroying the lpd. */
    cidConvertToPdfData(cid, title, pdata, pnbytes);
    return 0;
}


/*---------------------------------------------------------------------*
 *          Convert multiple images to pdf (one image per page)        *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaConvertToPdf()
 *
 * \param[in]    pixa          containing images all at the same resolution
 * \param[in]    res           override the resolution of each input image,
 *                             in ppi; use 0 to respect the resolution
 *                             embedded in the input images
 * \param[in]    scalefactor   scaling factor applied to each image; > 0.0
 * \param[in]    type          encoding type (L_JPEG_ENCODE, L_G4_ENCODE,
 *                             L_FLATE_ENCODE, L_JP2K_ENCODE, or
 *                             L_DEFAULT_ENCODE for default)
 * \param[in]    quality       for jpeg: 1-100; 0 for default (75)
 *                             for jp2k: 27-45; 0 for default (34)
 * \param[in]    title         [optional] pdf title; can be null
 * \param[in]    fileout       pdf file of all images
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The images are encoded with G4 if 1 bpp; JPEG if 8 bpp without
 *          colormap and many colors, or 32 bpp; FLATE for anything else.
 *      (2) The scalefactor must be > 0.0; otherwise it is set to 1.0.
 *      (3) Specifying one of the three encoding types for %type forces
 *          all images to be compressed with that type.  Use 0 to have
 *          the type determined for each image based on depth and whether
 *          or not it has a colormap.
 * </pre>
 */
l_ok
pixaConvertToPdf(PIXA        *pixa,
                 l_int32      res,
                 l_float32    scalefactor,
                 l_int32      type,
                 l_int32      quality,
                 const char  *title,
                 const char  *fileout)
{
l_uint8  *data;
l_int32   ret;
size_t    nbytes;

    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);

    ret = pixaConvertToPdfData(pixa, res, scalefactor, type, quality,
                               title, &data, &nbytes);
    if (ret) {
        LEPT_FREE(data);
        return ERROR_INT("conversion to pdf failed", __func__, 1);
    }

    ret = l_binaryWrite(fileout, "w", data, nbytes);
    LEPT_FREE(data);
    if (ret)
        L_ERROR("pdf data not written to file\n", __func__);
    return ret;
}


/*!
 * \brief   pixaConvertToPdfData()
 *
 * \param[in]    pixa           containing images all at the same resolution
 * \param[in]    res            input resolution of all images
 * \param[in]    scalefactor    scaling factor applied to each image; > 0.0; <50
 * \param[in]    type           encoding type (L_JPEG_ENCODE, L_G4_ENCODE,
 *                              L_FLATE_ENCODE, L_JP2K_ENCODE, or
 *                              L_DEFAULT_ENCODE for default)
 * \param[in]    quality        for jpeg: 1-100; 0 for default (75)
 *                              for jp2k: 27-45; 0 for default (34)
 * \param[in]    title          [optional] pdf title; can be null
 * \param[out]   pdata          output pdf data of all images
 * \param[out]   pnbytes        size of output pdf data
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) See pixaConvertToPdf().
 * </pre>
 */
l_ok
pixaConvertToPdfData(PIXA        *pixa,
                     l_int32      res,
                     l_float32    scalefactor,
                     l_int32      type,
                     l_int32      quality,
                     const char  *title,
                     l_uint8    **pdata,
                     size_t      *pnbytes)
{
l_uint8  *imdata;
l_int32   i, n, ret, scaledres, pagetype;
size_t    imbytes;
L_BYTEA  *ba;
PIX      *pixs, *pix;
L_PTRA   *pa_data;

    if (!pdata)
        return ERROR_INT("&data not defined", __func__, 1);
    *pdata = NULL;
    if (!pnbytes)
        return ERROR_INT("&nbytes not defined", __func__, 1);
    *pnbytes = 0;
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);
    if (scalefactor <= 0.0) scalefactor = 1.0;
    if (scalefactor >= 50.0)
        return ERROR_INT("scalefactor too large", __func__, 1);
    if (type != L_DEFAULT_ENCODE && type != L_JPEG_ENCODE &&
        type != L_G4_ENCODE && type != L_FLATE_ENCODE &&
        type != L_JP2K_ENCODE) {
        L_WARNING("invalid compression type; using per-page default\n",
                  __func__);
        type = L_DEFAULT_ENCODE;
    }
    if (quality < 0 || quality > 100)
        return ERROR_INT("invalid quality", __func__, 1);

        /* Generate all the encoded pdf strings */
    n = pixaGetCount(pixa);
    pa_data = ptraCreate(n);
    for (i = 0; i < n; i++) {
        if ((pixs = pixaGetPix(pixa, i, L_CLONE)) == NULL) {
            L_ERROR("pixs[%d] not retrieved\n", __func__, i);
            continue;
        }
        if (scalefactor != 1.0)
            pix = pixScale(pixs, scalefactor, scalefactor);
        else
            pix = pixClone(pixs);
        pixDestroy(&pixs);
        if (!pix) {
            L_ERROR("pix[%d] not made\n", __func__, i);
            continue;
        }
        scaledres = (l_int32)(res * scalefactor);

            /* Select the encoding type */
        if (type != L_DEFAULT_ENCODE) {
            pagetype = type;
        } else if (selectDefaultPdfEncoding(pix, &pagetype) != 0) {
            L_ERROR("encoding type selection failed for pix[%d]\n",
                        __func__, i);
            pixDestroy(&pix);
            continue;
        }

        ret = pixConvertToPdfData(pix, pagetype, quality, &imdata, &imbytes,
                                  0, 0, scaledres, title, NULL, 0);
        pixDestroy(&pix);
        if (ret) {
            LEPT_FREE(imdata);
            L_ERROR("pdf encoding failed for pix[%d]\n", __func__, i);
            continue;
        }
        ba = l_byteaInitFromMem(imdata, imbytes);
        LEPT_FREE(imdata);
        ptraAdd(pa_data, ba);
    }
    ptraGetActualCount(pa_data, &n);
    if (n == 0) {
        L_ERROR("no pdf files made\n", __func__);
        ptraDestroy(&pa_data, FALSE, FALSE);
        return 1;
    }

        /* Concatenate them */
    ret = ptraConcatenatePdfToData(pa_data, NULL, pdata, pnbytes);

    ptraGetActualCount(pa_data, &n);  /* recalculate in case it changes */
    for (i = 0; i < n; i++) {
        ba = (L_BYTEA *)ptraRemove(pa_data, i, L_NO_COMPACTION);
        l_byteaDestroy(&ba);
    }
    ptraDestroy(&pa_data, FALSE, FALSE);
    return ret;
}


/*---------------------------------------------------------------------*
 *                Single page, multi-image converters                  *
 *---------------------------------------------------------------------*/
/*!
 * \brief   convertToPdf()
 *
 * \param[in]      filein       input image file -- any format
 * \param[in]      type         encoding type (L_JPEG_ENCODE, L_G4_ENCODE,
 *                              L_FLATE_ENCODE, or L_JP2K_ENCODE)
 * \param[in]      quality      for jpeg: 1-100; 0 for default (75)
 *                              for jp2k: 27-45; 0 for default (34)
 * \param[in]      fileout      output pdf file; only required on last
 *                              image on page
 * \param[in]      x, y         location of lower-left corner of image,
 *                              in pixels, relative to the PostScript origin
 *                              (0,0) at the lower-left corner of the page
 * \param[in]      res          override the resolution of the input image,
 *                              in ppi; use 0 to respect the resolution
 *                              embedded in the input images
 * \param[in]      title        [optional] pdf title; can be null
 * \param[in,out]  plpd         ptr to lpd, which is created on the first
 *                              invocation and returned until last image is
 *                              processed, at which time it is destroyed
 * \param[in]      position     in image sequence: L_FIRST_IMAGE, L_NEXT_IMAGE,
 *                              L_LAST_IMAGE
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) To wrap only one image in pdf, input %plpd = NULL, and
 *          the value of %position will be ignored:
 *            convertToPdf(...  type, quality, x, y, res, NULL, 0);
 *      (2) To wrap multiple images on a single pdf page, this is called
 *          once for each successive image.  Do it this way:
 *            L_PDF_DATA   *lpd;
 *            convertToPdf(...  type, quality, x, y, res, &lpd, L_FIRST_IMAGE);
 *            convertToPdf(...  type, quality, x, y, res, &lpd, L_NEXT_IMAGE);
 *            ...
 *            convertToPdf(...  type, quality, x, y, res, &lpd, L_LAST_IMAGE);
 *          This will write the result to the value of %fileout specified
 *          in the first call; succeeding values of %fileout are ignored.
 *          On the last call: the pdf data bytes are computed and written
 *          to %fileout, lpd is destroyed internally, and the returned
 *          value of lpd is null.  So the client has nothing to clean up.
 *      (3) (a) Set %res == 0 to respect the resolution embedded in the
 *              image file.  If no resolution is embedded, it will be set
 *              to the default value.
 *          (b) Set %res to some other value to override the file resolution.
 *      (4) (a) If the input %res and the resolution of the output device
 *              are equal, the image will be "displayed" at the same size
 *              as the original.
 *          (b) If the input %res is 72, the output device will render
 *              the image at 1 pt/pixel.
 *          (c) Some possible choices for the default input pix resolution are:
 *                 72 ppi     Render pix on any output device at one pt/pixel
 *                 96 ppi     Windows default for generated display images
 *                300 ppi     Typical default for scanned images.
 *              We choose 300, which is sensible for rendering page images.
 *              However,  images come from a variety of sources, and
 *              some are explicitly created for viewing on a display.
 * </pre>
 */
l_ok
convertToPdf(const char   *filein,
             l_int32       type,
             l_int32       quality,
             const char   *fileout,
             l_int32       x,
             l_int32       y,
             l_int32       res,
             const char   *title,
             L_PDF_DATA  **plpd,
             l_int32       position)
{
l_uint8  *data;
l_int32   ret;
size_t    nbytes;

    if (!filein)
        return ERROR_INT("filein not defined", __func__, 1);
    if (!plpd || (position == L_LAST_IMAGE)) {
        if (!fileout)
            return ERROR_INT("fileout not defined", __func__, 1);
    }

    if (convertToPdfData(filein, type, quality, &data, &nbytes, x, y,
                         res, title, plpd, position))
        return ERROR_INT("pdf data not made", __func__, 1);

    if (!plpd || (position == L_LAST_IMAGE)) {
        ret = l_binaryWrite(fileout, "w", data, nbytes);
        LEPT_FREE(data);
        if (ret)
            return ERROR_INT("pdf data not written to file", __func__, 1);
    }

    return 0;
}


/*!
 * \brief   convertImageDataToPdf()
 *
 * \param[in]      imdata       array of formatted image data; e.g., png, jpeg
 * \param[in]      size         size of image data
 * \param[in]      type         encoding type (L_JPEG_ENCODE, L_G4_ENCODE,
 *                              L_FLATE_ENCODE, or L_JP2K_ENCODE)
 * \param[in]      quality      for jpeg: 1-100; 0 for default (75)
 *                              for jp2k: 27-45; 0 for default (34)
 * \param[in]      fileout      output pdf file; only required on last
 *                              image on page
 * \param[in]      x, y         location of lower-left corner of image,
 *                              in pixels, relative to the PostScript origin
 *                              (0,0) at the lower-left corner of the page
 * \param[in]      res          override the resolution of the input image,
 *                              in ppi; use 0 to respect the resolution
 *                              embedded in the input images
 * \param[in]      title        [optional] pdf title; can be null
 * \param[in,out]  plpd         ptr to lpd, which is created on the first
 *                              invocation and returned until last image is
 *                              processed, at which time it is destroyed
 * \param[in]      position     in image sequence: L_FIRST_IMAGE, L_NEXT_IMAGE,
 *                              L_LAST_IMAGE
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If %res == 0 and the input resolution field is 0,
 *          this will use DefaultInputRes.
 *      (2) See comments in convertToPdf().
 * </pre>
 */
l_ok
convertImageDataToPdf(l_uint8      *imdata,
                      size_t        size,
                      l_int32       type,
                      l_int32       quality,
                      const char   *fileout,
                      l_int32       x,
                      l_int32       y,
                      l_int32       res,
                      const char   *title,
                      L_PDF_DATA  **plpd,
                      l_int32       position)
{
l_int32  ret;
PIX     *pix;

    if (!imdata)
        return ERROR_INT("image data not defined", __func__, 1);
    if (!plpd || (position == L_LAST_IMAGE)) {
        if (!fileout)
            return ERROR_INT("fileout not defined", __func__, 1);
    }

    if ((pix = pixReadMem(imdata, size)) == NULL)
        return ERROR_INT("pix not read", __func__, 1);
    if (type != L_JPEG_ENCODE && type != L_G4_ENCODE &&
        type != L_FLATE_ENCODE && type != L_JP2K_ENCODE) {
        selectDefaultPdfEncoding(pix, &type);
    }
    ret = pixConvertToPdf(pix, type, quality, fileout, x, y, res,
                          title, plpd, position);
    pixDestroy(&pix);
    return ret;
}


/*!
 * \brief   convertToPdfData()
 *
 * \param[in]      filein       input image file -- any format
 * \param[in]      type         encoding type (L_JPEG_ENCODE, L_G4_ENCODE,
 *                              L_FLATE_ENCODE, or L_JP2K_ENCODE)
 * \param[in]      quality      for jpeg: 1-100; 0 for default (75)
 *                              for jp2k: 27-45; 0 for default (34)
 * \param[out]     pdata        pdf data in memory
 * \param[out]     pnbytes      number of bytes in pdf data
 * \param[in]      x, y         location of lower-left corner of image,
 *                              in pixels, relative to the PostScript origin
 *                              (0,0) at the lower-left corner of the page
 * \param[in]      res          override the resolution of the input image,
 *                              in ppi; use 0 to respect the resolution
 *                              embedded in the input images
 * \param[in]      title        [optional] pdf title; can be null
 * \param[in,out]  plpd         ptr to lpd, which is created on the first
 *                              invocation and returned until last image is
 *                              processed, at which time it is destroyed
 * \param[in]      position     in image sequence: L_FIRST_IMAGE, L_NEXT_IMAGE,
 *                              L_LAST_IMAGE
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If %res == 0 and the input resolution field is 0,
 *          this will use DefaultInputRes.
 *      (2) See comments in convertToPdf().
 * </pre>
 */
l_ok
convertToPdfData(const char   *filein,
                 l_int32       type,
                 l_int32       quality,
                 l_uint8     **pdata,
                 size_t       *pnbytes,
                 l_int32       x,
                 l_int32       y,
                 l_int32       res,
                 const char   *title,
                 L_PDF_DATA  **plpd,
                 l_int32       position)
{
PIX  *pix;

    if (!pdata)
        return ERROR_INT("&data not defined", __func__, 1);
    *pdata = NULL;
    if (!pnbytes)
        return ERROR_INT("&nbytes not defined", __func__, 1);
    *pnbytes = 0;
    if (!filein)
        return ERROR_INT("filein not defined", __func__, 1);

    if ((pix = pixRead(filein)) == NULL)
        return ERROR_INT("pix not made", __func__, 1);

    pixConvertToPdfData(pix, type, quality, pdata, pnbytes,
                        x, y, res, title, plpd, position);
    pixDestroy(&pix);
    return 0;
}


/*!
 * \brief   convertImageDataToPdfData()
 *
 * \param[in]    imdata       array of formatted image data; e.g., png, jpeg
 * \param[in]    size         size of image data
 * \param[in]    type         encoding type (L_JPEG_ENCODE, L_G4_ENCODE,
 *                            L_FLATE_ENCODE, or L_JP2K_ENCODE)
 * \param[in]    quality      for jpeg: 1-100; 0 for default (75)
 *                            for jp2k: 27-45; 0 for default (34)
 * \param[out]   pdata        pdf data in memory
 * \param[out]   pnbytes      number of bytes in pdf data
 * \param[in]    x, y         location of lower-left corner of image,
 *                            in pixels, relative to the PostScript origin
 *                            (0,0) at the lower-left corner of the page
 * \param[in]    res          override the resolution of the input image,
 *                            in ppi; use 0 to respect the resolution
 *                            embedded in the input images
 * \param[in]    title        [optional] pdf title; can be null
 * \param[out]   plpd         ptr to lpd, which is created on the first
 *                            invocation and returned until last image is
 *                            processed, at which time it is destroyed
 * \param[in]    position     in image sequence: L_FIRST_IMAGE, L_NEXT_IMAGE,
 *                            L_LAST_IMAGE
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If %res == 0 and the input resolution field is 0,
 *          this will use DefaultInputRes.
 *      (2) See comments in convertToPdf().
 * </pre>
 */
l_ok
convertImageDataToPdfData(l_uint8      *imdata,
                          size_t        size,
                          l_int32       type,
                          l_int32       quality,
                          l_uint8     **pdata,
                          size_t       *pnbytes,
                          l_int32       x,
                          l_int32       y,
                          l_int32       res,
                          const char   *title,
                          L_PDF_DATA  **plpd,
                          l_int32       position)
{
l_int32  ret;
PIX     *pix;

    if (!pdata)
        return ERROR_INT("&data not defined", __func__, 1);
    *pdata = NULL;
    if (!pnbytes)
        return ERROR_INT("&nbytes not defined", __func__, 1);
    *pnbytes = 0;
    if (!imdata)
        return ERROR_INT("image data not defined", __func__, 1);
    if (plpd) {  /* part of multi-page invocation */
        if (position == L_FIRST_IMAGE)
            *plpd = NULL;
    }

    if ((pix = pixReadMem(imdata, size)) == NULL)
        return ERROR_INT("pix not read", __func__, 1);
    if (type != L_JPEG_ENCODE && type != L_G4_ENCODE &&
        type != L_FLATE_ENCODE && type != L_JP2K_ENCODE) {
        selectDefaultPdfEncoding(pix, &type);
    }
    ret = pixConvertToPdfData(pix, type, quality, pdata, pnbytes,
                              x, y, res, title, plpd, position);
    pixDestroy(&pix);
    return ret;
}


/*!
 * \brief   pixConvertToPdf()
 *
 * \param[in]      pix
 * \param[in]      type         encoding type (L_JPEG_ENCODE, L_G4_ENCODE,
 *                              L_FLATE_ENCODE, L_JP2K_ENCODE)
 * \param[in]      quality      for jpeg: 1-100; 0 for default (75)
 *                              for jp2k: 27-45; 0 for default (34)
 * \param[in]      fileout      output pdf file; only required on last
 *                              image on page
 * \param[in]      x, y         location of lower-left corner of image,
 *                              in pixels, relative to the PostScript origin
 *                              (0,0) at the lower-left corner of the page
 * \param[in]      res          override the resolution of the input image,
 *                              in ppi; use 0 to respect the resolution
 *                              embedded in the input images
 * \param[in]      title        [optional] pdf title; can be null
 * \param[in,out]  plpd         ptr to lpd, which is created on the first
 *                              invocation and returned until last image is
 *                              processed, at which time it is destroyed
 * \param[in]      position     in image sequence: L_FIRST_IMAGE, L_NEXT_IMAGE,
 *                              L_LAST_IMAGE
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If %res == 0 and the input resolution field is 0,
 *          this will use DefaultInputRes.
 *      (2) This only writes data to fileout if it is the last
 *          image to be written on the page.
 *      (3) See comments in convertToPdf().
 * </pre>
 */
l_ok
pixConvertToPdf(PIX          *pix,
                l_int32       type,
                l_int32       quality,
                const char   *fileout,
                l_int32       x,
                l_int32       y,
                l_int32       res,
                const char   *title,
                L_PDF_DATA  **plpd,
                l_int32       position)
{
l_uint8  *data;
l_int32   ret;
size_t    nbytes;

    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    if (!plpd || (position == L_LAST_IMAGE)) {
        if (!fileout)
            return ERROR_INT("fileout not defined", __func__, 1);
    }

    if (pixConvertToPdfData(pix, type, quality, &data, &nbytes,
                            x, y, res, title, plpd, position)) {
        LEPT_FREE(data);
        return ERROR_INT("pdf data not made", __func__, 1);
    }

    if (!plpd || (position == L_LAST_IMAGE)) {
        ret = l_binaryWrite(fileout, "w", data, nbytes);
        LEPT_FREE(data);
        if (ret)
            return ERROR_INT("pdf data not written to file", __func__, 1);
    }
    return 0;
}


/*!
 * \brief   pixWriteStreamPdf()
 *
 * \param[in]    fp       file stream opened for writing
 * \param[in]    pix      all depths, cmap OK
 * \param[in]    res      override the resolution of the input image, in ppi;
 *                        use 0 to respect the resolution embedded in the input
 * \param[in]    title    [optional] pdf title; can be null
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is the simplest interface for writing a single image
 *          with pdf encoding to a stream.  It uses G4 encoding for 1 bpp,
 *          JPEG encoding for 8 bpp (no cmap) and 32 bpp, and FLATE
 *          encoding for everything else.
 * </pre>
 */
l_ok
pixWriteStreamPdf(FILE        *fp,
                  PIX         *pix,
                  l_int32      res,
                  const char  *title)
{
l_uint8  *data;
size_t    nbytes, nbytes_written;

    if (!fp)
        return ERROR_INT("stream not opened", __func__, 1);
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    if (pixWriteMemPdf(&data, &nbytes, pix, res, title) != 0) {
        LEPT_FREE(data);
        return ERROR_INT("pdf data not made", __func__, 1);
    }

    nbytes_written = fwrite(data, 1, nbytes, fp);
    LEPT_FREE(data);
    if (nbytes != nbytes_written)
        return ERROR_INT("failure writing pdf data to stream", __func__, 1);
    return 0;
}


/*!
 * \brief   pixWriteMemPdf()
 *
 * \param[out]   pdata      pdf as byte array
 * \param[out]   pnbytes    number of bytes in pdf array
 * \param[in]    pix        all depths, cmap OK
 * \param[in]    res        override the resolution of the input image, in ppi;
 *                          use 0 to respect the res embedded in the input
 * \param[in]    title      [optional] pdf title; can be null
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is the simplest interface for writing a single image
 *          with pdf encoding to memory.  It uses G4 encoding for 1 bpp,
 *          and makes a guess whether to use JPEG or FLATE encoding for
 *          everything else.
 * </pre>
 */
l_ok
pixWriteMemPdf(l_uint8    **pdata,
               size_t      *pnbytes,
               PIX         *pix,
               l_int32      res,
               const char  *title)
{
l_int32  ret, type;

    if (pdata) *pdata = NULL;
    if (pnbytes) *pnbytes = 0;
    if (!pdata || !pnbytes)
        return ERROR_INT("&data or &nbytes not defined", __func__, 1);
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    selectDefaultPdfEncoding(pix, &type);
    ret = pixConvertToPdfData(pix, type, 75, pdata, pnbytes,
                              0, 0, res, title, NULL, 0);
    if (ret)
        return ERROR_INT("pdf data not made", __func__, 1);
    return 0;
}


/*---------------------------------------------------------------------*
 *            Segmented multi-page, multi-image converter              *
 *---------------------------------------------------------------------*/
/*!
 * \brief   convertSegmentedFilesToPdf()
 *
 * \param[in]    dirname       directory name containing images
 * \param[in]    substr        [optional] substring filter on filenames;
 *                             can be null
 * \param[in]    res           input resolution of all images
 * \param[in]    type          compression type for non-image regions; the
 *                             image regions are always compressed with
 *                             L_JPEG_ENCODE
 * \param[in]    thresh        used for converting gray --> 1 bpp with
 *                             L_G4_ENCODE
 * \param[in]    baa           [optional] boxaa of image regions
 * \param[in]    quality       used for JPEG only; 0 for default (75)
 * \param[in]    scalefactor   scaling factor applied to each image region
 * \param[in]    title         [optional] pdf title; can be null
 * \param[in]    fileout       pdf file of all images
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If %substr is not NULL, only image filenames that contain
 *          the substring can be used.  If %substr == NULL, all files
 *          in the directory are used.
 *      (2) The files in the directory, after optional filtering by
 *          the substring, are lexically sorted in increasing order
 *          before concatenation.
 *      (3) The images are encoded with G4 if 1 bpp; JPEG if 8 bpp without
 *          colormap and many colors, or 32 bpp; FLATE for anything else.
 *      (4) The boxaa, if it exists, contains one boxa of "image regions"
 *          for each image file.  The boxa must be aligned with the
 *          sorted set of images.
 *      (5) The scalefactor is applied to each image region.  It is
 *          typically < 1.0, to save bytes in the final pdf, because
 *          the resolution is often not critical in non-text regions.
 *      (6) If the non-image regions have pixel depth > 1 and the encoding
 *          type is G4, they are automatically scaled up by 2x and
 *          thresholded.  Otherwise, no scaling is performed on them.
 *      (7) Note that this function can be used to generate multipage
 *          G4 compressed pdf from any input, by using %boxaa == NULL
 *          and %type == L_G4_ENCODE.
 * </pre>
 */
l_ok
convertSegmentedFilesToPdf(const char  *dirname,
                           const char  *substr,
                           l_int32      res,
                           l_int32      type,
                           l_int32      thresh,
                           BOXAA       *baa,
                           l_int32      quality,
                           l_float32    scalefactor,
                           const char  *title,
                           const char  *fileout)
{
char     *fname;
l_uint8  *imdata, *data;
l_int32   i, npages, nboxa, nboxes, ret;
size_t    imbytes, databytes;
BOXA     *boxa;
L_BYTEA  *ba;
L_PTRA   *pa_data;
SARRAY   *sa;

    if (!dirname)
        return ERROR_INT("dirname not defined", __func__, 1);
    if (!fileout)
        return ERROR_INT("fileout not defined", __func__, 1);

    if ((sa = getNumberedPathnamesInDirectory(dirname, substr, 0, 0, 10000))
            == NULL)
        return ERROR_INT("sa not made", __func__, 1);

    npages = sarrayGetCount(sa);
        /* If necessary, extend the boxaa, which is page-aligned with
         * the image files, to be as large as the set of images. */
    if (baa) {
        nboxa = boxaaGetCount(baa);
        if (nboxa < npages) {
            boxa = boxaCreate(1);
            boxaaExtendWithInit(baa, npages, boxa);
            boxaDestroy(&boxa);
        }
    }

        /* Generate and save all the encoded pdf strings */
    pa_data = ptraCreate(npages);
    for (i = 0; i < npages; i++) {
        fname = sarrayGetString(sa, i, L_NOCOPY);
        if (!strcmp(fname, "")) continue;
        boxa = NULL;
        if (baa) {
            boxa = boxaaGetBoxa(baa, i, L_CLONE);
            nboxes = boxaGetCount(boxa);
            if (nboxes == 0)
                boxaDestroy(&boxa);
        }
        ret = convertToPdfDataSegmented(fname, res, type, thresh, boxa,
                                        quality, scalefactor, title,
                                        &imdata, &imbytes);
        boxaDestroy(&boxa);  /* safe; in case nboxes > 0 */
        if (ret) {
            L_ERROR("pdf encoding failed for %s\n", __func__, fname);
            continue;
        }
        ba = l_byteaInitFromMem(imdata, imbytes);
        if (imdata) LEPT_FREE(imdata);
        ptraAdd(pa_data, ba);
    }
    sarrayDestroy(&sa);

    ptraGetActualCount(pa_data, &npages);
    if (npages == 0) {
        L_ERROR("no pdf files made\n", __func__);
        ptraDestroy(&pa_data, FALSE, FALSE);
        return 1;
    }

        /* Concatenate */
    ret = ptraConcatenatePdfToData(pa_data, NULL, &data, &databytes);

        /* Clean up */
    ptraGetActualCount(pa_data, &npages);  /* recalculate in case it changes */
    for (i = 0; i < npages; i++) {
        ba = (L_BYTEA *)ptraRemove(pa_data, i, L_NO_COMPACTION);
        l_byteaDestroy(&ba);
    }
    ptraDestroy(&pa_data, FALSE, FALSE);

    if (ret) {
        if (data) LEPT_FREE(data);
        return ERROR_INT("pdf data not made", __func__, 1);
    }

    ret = l_binaryWrite(fileout, "w", data, databytes);
    LEPT_FREE(data);
    if (ret)
        L_ERROR("pdf data not written to file\n", __func__);
    return ret;
}


/*!
 * \brief   convertNumberedMasksToBoxaa()
 *
 * \param[in]    dirname   directory name containing mask images
 * \param[in]    substr    [optional] substring filter on filenames;
 *                         can be null 
 * \param[in]    numpre    number of characters in name before number
 * \param[in]    numpost   number of characters in name after number,
 *                         up to a dot before an extension
 * \return  boxaa of mask regions, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is conveniently used to generate the input boxaa
 *          for convertSegmentedFilesToPdf().  It guarantees that the
 *          boxa will be aligned with the page images, even if some
 *          of the boxa are empty.
 * </pre>
 */
BOXAA *
convertNumberedMasksToBoxaa(const char  *dirname,
                            const char  *substr,
                            l_int32      numpre,
                            l_int32      numpost)
{
char    *fname;
l_int32  i, n;
BOXA    *boxa;
BOXAA   *baa;
PIX     *pix;
SARRAY  *sa;

    if (!dirname)
        return (BOXAA *)ERROR_PTR("dirname not defined", __func__, NULL);

    if ((sa = getNumberedPathnamesInDirectory(dirname, substr, numpre,
                                              numpost, 10000)) == NULL)
        return (BOXAA *)ERROR_PTR("sa not made", __func__, NULL);

        /* Generate and save all the encoded pdf strings */
    n = sarrayGetCount(sa);
    baa = boxaaCreate(n);
    boxa = boxaCreate(1);
    boxaaInitFull(baa, boxa);
    boxaDestroy(&boxa);
    for (i = 0; i < n; i++) {
        fname = sarrayGetString(sa, i, L_NOCOPY);
        if (!strcmp(fname, "")) continue;
        if ((pix = pixRead(fname)) == NULL) {
            L_WARNING("invalid image on page %d\n", __func__, i);
            continue;
        }
        boxa = pixConnComp(pix, NULL, 8);
        boxaaReplaceBoxa(baa, i, boxa);
        pixDestroy(&pix);
    }

    sarrayDestroy(&sa);
    return baa;
}


/*---------------------------------------------------------------------*
 *            Segmented single page, multi-image converters            *
 *---------------------------------------------------------------------*/
/*!
 * \brief   convertToPdfSegmented()
 *
 * \param[in]    filein        input image file -- any format
 * \param[in]    res           input image resolution; typ. 300 ppi;
 *                             use 0 for default
 * \param[in]    type          compression type for non-image regions; image
 *                             regions are always compressed with L_JPEG_ENCODE
 * \param[in]    thresh        for converting gray --> 1 bpp with L_G4_ENCODE
 * \param[in]    boxa          [optional] of image regions; can be null
 * \param[in]    quality       used for jpeg image regions; 0 for default
 * \param[in]    scalefactor   used for jpeg regions; must be <= 1.0
 * \param[in]    title         [optional] pdf title; can be null
 * \param[in]    fileout       output pdf file
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If there are no image regions, set %boxa == NULL;
 *          %quality and %scalefactor are ignored.
 *      (2) Typically, %scalefactor is < 1.0, because the image regions
 *          can be rendered at a lower resolution (for better compression)
 *          than the text regions.  If %scalefactor == 0, we use 1.0.
 *          If the input image is 1 bpp and scalefactor < 1.0, we
 *          use scaleToGray() to downsample the image regions to gray
 *          before compressing them.
 *      (3) If the compression type for non-image regions is L_G4_ENCODE
 *          and bpp > 1, the image is upscaled 2x and thresholded
 *          to 1 bpp.  That is the only situation where %thresh is used.
 *      (4) The parameter %quality is only used for image regions.
 *          If %type == L_JPEG_ENCODE, default jpeg quality (75) is
 *          used for the non-image regions.
 *      (5) Processing matrix for non-image regions.
 *
 *          Input           G4              JPEG                FLATE
 *          ----------|---------------------------------------------------
 *          1 bpp     |  1x, 1 bpp       1x flate, 1 bpp     1x, 1 bpp
 *                    |
 *          cmap      |  2x, 1 bpp       1x flate, cmap      1x, cmap
 *                    |
 *          2,4 bpp   |  2x, 1 bpp       1x flate            1x, 2,4 bpp
 *          no cmap   |                  2,4 bpp
 *                    |
 *          8,32 bpp  |  2x, 1 bpp       1x (jpeg)           1x, 8,32 bpp
 *          no cmap   |                  8,32 bpp
 *
 *          Summary:
 *          (a) if G4 is requested, G4 is used, with 2x upscaling
 *              for all cases except 1 bpp.
 *          (b) if JPEG is requested, use flate encoding for all cases
 *              except 8 bpp without cmap and 32 bpp (rgb).
 *          (c) if FLATE is requested, use flate with no transformation
 *              of the raster data.
 *      (6) Calling options/sequence for these functions:
 *              file  -->  file      (convertToPdfSegmented)
 *                  pix  -->  file      (pixConvertToPdfSegmented)
 *                      pix  -->  data      (pixConvertToPdfDataSegmented)
 *              file  -->  data      (convertToPdfDataSegmented)
 *                      pix  -->  data      (pixConvertToPdfDataSegmented)
 * </pre>
 */
l_ok
convertToPdfSegmented(const char  *filein,
                      l_int32      res,
                      l_int32      type,
                      l_int32      thresh,
                      BOXA        *boxa,
                      l_int32      quality,
                      l_float32    scalefactor,
                      const char  *title,
                      const char  *fileout)
{
l_int32  ret;
PIX     *pixs;

    if (!filein)
        return ERROR_INT("filein not defined", __func__, 1);
    if (!fileout)
        return ERROR_INT("fileout not defined", __func__, 1);
    if (type != L_G4_ENCODE && type != L_JPEG_ENCODE &&
        type != L_FLATE_ENCODE)
        return ERROR_INT("invalid conversion type", __func__, 1);
    if (boxa && scalefactor > 1.0) {
        L_WARNING("setting scalefactor to 1.0\n", __func__);
        scalefactor = 1.0;
    }

    if ((pixs = pixRead(filein)) == NULL)
        return ERROR_INT("pixs not made", __func__, 1);

    ret = pixConvertToPdfSegmented(pixs, res, type, thresh, boxa, quality,
                                   scalefactor, title, fileout);
    pixDestroy(&pixs);
    return ret;
}


/*!
 * \brief   pixConvertToPdfSegmented()
 *
 * \param[in]    pixs          any depth, cmap OK
 * \param[in]    res           input image resolution; typ. 300 ppi;
 *                             use 0 for default
 * \param[in]    type          compression type for non-image regions; image
 *                             regions are always compressed with L_JPEG_ENCODE
 * \param[in]    thresh        for converting gray --> 1 bpp with L_G4_ENCODE
 * \param[in]    boxa          [optional] of image regions; can be null
 * \param[in]    quality       used for jpeg image regions; 0 for default
 * \param[in]    scalefactor   used for jpeg regions; must be <= 1.0
 * \param[in]    title         [optional] pdf title; can be null
 * \param[in]    fileout       output pdf file
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) See convertToPdfSegmented() for details.
 * </pre>
 */
l_ok
pixConvertToPdfSegmented(PIX         *pixs,
                         l_int32      res,
                         l_int32      type,
                         l_int32      thresh,
                         BOXA        *boxa,
                         l_int32      quality,
                         l_float32    scalefactor,
                         const char  *title,
                         const char  *fileout)
{
l_uint8  *data;
l_int32   ret;
size_t    nbytes;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (!fileout)
        return ERROR_INT("fileout not defined", __func__, 1);
    if (type != L_G4_ENCODE && type != L_JPEG_ENCODE &&
        type != L_FLATE_ENCODE)
        return ERROR_INT("invalid conversion type", __func__, 1);
    if (boxa && scalefactor > 1.0) {
        L_WARNING("setting scalefactor to 1.0\n", __func__);
        scalefactor = 1.0;
    }

    ret = pixConvertToPdfDataSegmented(pixs, res, type, thresh, boxa, quality,
                                       scalefactor, title, &data, &nbytes);
    if (ret)
        return ERROR_INT("pdf generation failure", __func__, 1);

    ret = l_binaryWrite(fileout, "w", data, nbytes);
    if (data) LEPT_FREE(data);
    return ret;
}


/*!
 * \brief   convertToPdfDataSegmented()
 *
 * \param[in]    filein        input image file -- any format
 * \param[in]    res           input image resolution; typ. 300 ppi;
 *                             use 0 for default
 * \param[in]    type          compression type for non-image regions; image
 *                             regions are always compressed with L_JPEG_ENCODE
 * \param[in]    thresh        for converting gray --> 1 bpp with L_G4_ENCODE
 * \param[in]    boxa          [optional] image regions; can be null
 * \param[in]    quality       used for jpeg image regions; 0 for default
 * \param[in]    scalefactor   used for jpeg regions; must be <= 1.0
 * \param[in]    title         [optional] pdf title; can be null
 * \param[out]   pdata         pdf data in memory
 * \param[out]   pnbytes       number of bytes in pdf data
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If there are no image regions, set %boxa == NULL;
 *          %quality and %scalefactor are ignored.
 *      (2) Typically, %scalefactor is < 1.0.  The image regions are
 * </pre>
 */
l_ok
convertToPdfDataSegmented(const char  *filein,
                          l_int32      res,
                          l_int32      type,
                          l_int32      thresh,
                          BOXA        *boxa,
                          l_int32      quality,
                          l_float32    scalefactor,
                          const char  *title,
                          l_uint8    **pdata,
                          size_t      *pnbytes)
{
l_int32  ret;
PIX     *pixs;

    if (!pdata)
        return ERROR_INT("&data not defined", __func__, 1);
    *pdata = NULL;
    if (!pnbytes)
        return ERROR_INT("&nbytes not defined", __func__, 1);
    *pnbytes = 0;
    if (!filein)
        return ERROR_INT("filein not defined", __func__, 1);
    if (type != L_G4_ENCODE && type != L_JPEG_ENCODE &&
        type != L_FLATE_ENCODE)
        return ERROR_INT("invalid conversion type", __func__, 1);
    if (boxa && scalefactor > 1.0) {
        L_WARNING("setting scalefactor to 1.0\n", __func__);
        scalefactor = 1.0;
    }

    if ((pixs = pixRead(filein)) == NULL)
        return ERROR_INT("pixs not made", __func__, 1);

    ret = pixConvertToPdfDataSegmented(pixs, res, type, thresh, boxa,
                                       quality, scalefactor, title,
                                       pdata, pnbytes);
    pixDestroy(&pixs);
    return ret;
}


/*!
 * \brief   pixConvertToPdfDataSegmented()
 *
 * \param[in]    pixs          any depth, cmap OK
 * \param[in]    res           input image resolution; typ. 300 ppi;
 *                             use 0 for default
 * \param[in]    type          compression type for non-image regions; image
 *                             regions are always compressed with L_JPEG_ENCODE
 * \param[in]    thresh        for converting gray --> 1 bpp with L_G4_ENCODE
 * \param[in]    boxa          [optional] of image regions; can be null
 * \param[in]    quality       used for jpeg image regions; 0 for default
 * \param[in]    scalefactor   used for jpeg regions; must be <= 1.0
 * \param[in]    title         [optional] pdf title; can be null
 * \param[out]   pdata         pdf data in memory
 * \param[out]   pnbytes       number of bytes in pdf data
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) See convertToPdfSegmented() for details.
 * </pre>
 */
l_ok
pixConvertToPdfDataSegmented(PIX         *pixs,
                             l_int32      res,
                             l_int32      type,
                             l_int32      thresh,
                             BOXA        *boxa,
                             l_int32      quality,
                             l_float32    scalefactor,
                             const char  *title,
                             l_uint8    **pdata,
                             size_t      *pnbytes)
{
l_int32      i, nbox, seq, bx, by, bw, bh, upscale;
l_float32    scale;
BOX         *box, *boxc, *box2;
PIX         *pix, *pixt1, *pixt2, *pixt3, *pixt4, *pixt5, *pixt6;
PIXCMAP     *cmap;
L_PDF_DATA  *lpd;

    if (!pdata)
        return ERROR_INT("&data not defined", __func__, 1);
    *pdata = NULL;
    if (!pnbytes)
        return ERROR_INT("&nbytes not defined", __func__, 1);
    *pnbytes = 0;
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (type != L_G4_ENCODE && type != L_JPEG_ENCODE &&
        type != L_FLATE_ENCODE)
        return ERROR_INT("invalid conversion type", __func__, 1);
    if (boxa && (scalefactor <= 0.0 || scalefactor > 1.0)) {
        L_WARNING("setting scalefactor to 1.0\n", __func__);
        scalefactor = 1.0;
    }

        /* Adjust scalefactor so that the product with res gives an integer */
    if (res <= 0)
        res = DefaultInputRes;
    scale = (l_float32)((l_int32)(scalefactor * res + 0.5)) / (l_float32)res;
    cmap = pixGetColormap(pixs);

        /* Simple case: single image to be encoded */
    if (!boxa || boxaGetCount(boxa) == 0) {
        if (pixGetDepth(pixs) > 1 && type == L_G4_ENCODE) {
            if (cmap)
                pixt1 = pixRemoveColormap(pixs, REMOVE_CMAP_TO_GRAYSCALE);
            else
                pixt1 = pixConvertTo8(pixs, FALSE);
            pixt2 = pixScaleGray2xLIThresh(pixt1, thresh);
            pixConvertToPdfData(pixt2, type, quality, pdata, pnbytes,
                                0, 0, 2 * res, title, NULL, 0);
            pixDestroy(&pixt1);
            pixDestroy(&pixt2);
        } else {
            pixConvertToPdfData(pixs, type, quality, pdata, pnbytes,
                                0, 0, res, title, NULL, 0);
        }
        return 0;
    }

        /* Multiple images to be encoded.  If %type == L_G4_ENCODE,
         * jpeg encode a version of pixs that is blanked in the non-image
         * regions, and paint the scaled non-image part onto it through a mask.
         * Otherwise, we must put the non-image part down first and
         * then render all the image regions separately on top of it,
         * at their own resolution. */
    pixt1 = pixSetBlackOrWhiteBoxa(pixs, boxa, L_SET_WHITE);  /* non-image */
    nbox = boxaGetCount(boxa);
    if (type == L_G4_ENCODE) {
        pixt2 = pixCreateTemplate(pixs);  /* only image regions */
        pixSetBlackOrWhite(pixt2, L_SET_WHITE);
        for (i = 0; i < nbox; i++) {
             box = boxaGetBox(boxa, i, L_CLONE);
             pix = pixClipRectangle(pixs, box, &boxc);
             boxGetGeometry(boxc, &bx, &by, &bw, &bh);
             pixRasterop(pixt2, bx, by, bw, bh, PIX_SRC, pix, 0, 0);
             pixDestroy(&pix);
             boxDestroy(&box);
             boxDestroy(&boxc);
        }
        pixt3 = pixRemoveColormap(pixt2, REMOVE_CMAP_BASED_ON_SRC);
        if (pixGetDepth(pixt3) == 1)
            pixt4 = pixScaleToGray(pixt3, scale);
        else
            pixt4 = pixScale(pixt3, scale, scale);
        pixConvertToPdfData(pixt4, L_JPEG_ENCODE, quality, pdata, pnbytes,
                            0, 0, (l_int32)(scale * res), title,
                            &lpd, L_FIRST_IMAGE);

        if (pixGetDepth(pixt1) == 1) {
            pixt5 = pixClone(pixt1);
            upscale = 1;
        } else {
            pixt6 = pixConvertTo8(pixt1, 0);
            pixt5 = pixScaleGray2xLIThresh(pixt6, thresh);
            pixDestroy(&pixt6);
            upscale = 2;
        }
        pixConvertToPdfData(pixt5, L_G4_ENCODE, quality, pdata, pnbytes,
                            0, 0, upscale * res, title, &lpd, L_LAST_IMAGE);
        pixDestroy(&pixt2);
        pixDestroy(&pixt3);
        pixDestroy(&pixt4);
        pixDestroy(&pixt5);
    } else {
            /* Put the non-image part down first.  This is the full
               size of the page, so we can use it to find the page
               height in pixels, which is required for determining
               the LL corner of the image relative to the LL corner
               of the page. */
        pixConvertToPdfData(pixt1, type, quality, pdata, pnbytes, 0, 0,
                            res, title, &lpd, L_FIRST_IMAGE);
        for (i = 0; i < nbox; i++) {
            box = boxaGetBox(boxa, i, L_CLONE);
            pixt2 = pixClipRectangle(pixs, box, &boxc);
            pixt3 = pixRemoveColormap(pixt2, REMOVE_CMAP_BASED_ON_SRC);
            if (pixGetDepth(pixt3) == 1)
                pixt4 = pixScaleToGray(pixt3, scale);
            else
                pixt4 = pixScale(pixt3, scale, scale);
            box2 = boxTransform(boxc, 0, 0, scale, scale);
            boxGetGeometry(box2, &bx, &by, NULL, &bh);
            seq = (i == nbox - 1) ? L_LAST_IMAGE : L_NEXT_IMAGE;
            pixConvertToPdfData(pixt4, L_JPEG_ENCODE, quality, pdata, pnbytes,
                                bx, by, (l_int32)(scale * res), title,
                                &lpd, seq);
            pixDestroy(&pixt2);
            pixDestroy(&pixt3);
            pixDestroy(&pixt4);
            boxDestroy(&box);
            boxDestroy(&boxc);
            boxDestroy(&box2);
        }
    }

    pixDestroy(&pixt1);
    return 0;
}


/*---------------------------------------------------------------------*
 *                         Multi-page concatenation                    *
 *---------------------------------------------------------------------*/
/*!
 * \brief   concatenatePdf()
 *
 * \param[in]    dirname   directory name containing single-page pdf files
 * \param[in]    substr    [optional] substring filter on filenames;
 *                         can be null
 * \param[in]    fileout   concatenated pdf file
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This only works with leptonica-formatted single-page pdf files.
 *      (2) If %substr is not NULL, only filenames that contain
 *          the substring can be returned.  If %substr == NULL,
 *          none of the filenames are filtered out.
 *      (3) The files in the directory, after optional filtering by
 *          the substring, are lexically sorted in increasing order
 *          before concatenation.
 * </pre>
 */
l_ok
concatenatePdf(const char  *dirname,
               const char  *substr,
               const char  *fileout)
{
l_int32  ret;
SARRAY  *sa;

    if (!dirname)
        return ERROR_INT("dirname not defined", __func__, 1);
    if (!fileout)
        return ERROR_INT("fileout not defined", __func__, 1);

    if ((sa = getSortedPathnamesInDirectory(dirname, substr, 0, 0)) == NULL)
        return ERROR_INT("sa not made", __func__, 1);
    ret = saConcatenatePdf(sa, fileout);
    sarrayDestroy(&sa);
    return ret;
}


/*!
 * \brief   saConcatenatePdf()
 *
 * \param[in]    sa        string array of pathnames for single-page pdf files
 * \param[in]    fileout   concatenated pdf file
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This only works with leptonica-formatted single-page pdf files.
 * </pre>
 */
l_ok
saConcatenatePdf(SARRAY      *sa,
                 const char  *fileout)
{
l_uint8  *data;
l_int32   ret;
size_t    nbytes;

    if (!sa)
        return ERROR_INT("sa not defined", __func__, 1);
    if (!fileout)
        return ERROR_INT("fileout not defined", __func__, 1);

    ret = saConcatenatePdfToData(sa, &data, &nbytes);
    if (ret)
        return ERROR_INT("pdf data not made", __func__, 1);
    ret = l_binaryWrite(fileout, "w", data, nbytes);
    LEPT_FREE(data);
    return ret;
}


/*!
 * \brief   ptraConcatenatePdf()
 *
 * \param[in]    pa       array of pdf strings, each for a single-page pdf file
 * \param[in]    fileout  concatenated pdf file
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This only works with leptonica-formatted single-page pdf files.
 * </pre>
 */
l_ok
ptraConcatenatePdf(L_PTRA      *pa,
                   const char  *fileout)
{
l_uint8  *data;
l_int32   ret;
size_t    nbytes;

    if (!pa)
        return ERROR_INT("pa not defined", __func__, 1);
    if (!fileout)
        return ERROR_INT("fileout not defined", __func__, 1);

    ret = ptraConcatenatePdfToData(pa, NULL, &data, &nbytes);
    if (ret)
        return ERROR_INT("pdf data not made", __func__, 1);
    ret = l_binaryWrite(fileout, "w", data, nbytes);
    LEPT_FREE(data);
    return ret;
}


/*!
 * \brief   concatenatePdfToData()
 *
 * \param[in]    dirname   directory name containing single-page pdf files
 * \param[in]    substr    [optional] substring filter on filenames;
 *                         can be null
 * \param[out]   pdata     concatenated pdf data in memory
 * \param[out]   pnbytes   number of bytes in pdf data
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This only works with leptonica-formatted single-page pdf files.
 *      (2) If %substr is not NULL, only filenames that contain
 *          the substring can be returned.  If %substr == NULL,
 *          none of the filenames are filtered out.
 *      (3) The files in the directory, after optional filtering by
 *          the substring, are lexically sorted in increasing order
 *          before concatenation.
 * </pre>
 */
l_ok
concatenatePdfToData(const char  *dirname,
                     const char  *substr,
                     l_uint8    **pdata,
                     size_t      *pnbytes)
{
l_int32  ret;
SARRAY  *sa;

    if (!pdata)
        return ERROR_INT("&data not defined", __func__, 1);
    *pdata = NULL;
    if (!pnbytes)
        return ERROR_INT("&nbytes not defined", __func__, 1);
    *pnbytes = 0;
    if (!dirname)
        return ERROR_INT("dirname not defined", __func__, 1);

    if ((sa = getSortedPathnamesInDirectory(dirname, substr, 0, 0)) == NULL)
        return ERROR_INT("sa not made", __func__, 1);
    ret = saConcatenatePdfToData(sa, pdata, pnbytes);
    sarrayDestroy(&sa);
    return ret;
}


/*!
 * \brief   saConcatenatePdfToData()
 *
 * \param[in]    sa        string array of pathnames for single-page pdf files
 * \param[out]   pdata     concatenated pdf data in memory
 * \param[out]   pnbytes   number of bytes in pdf data
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This only works with leptonica-formatted single-page pdf files.
 * </pre>
 */
l_ok
saConcatenatePdfToData(SARRAY    *sa,
                       l_uint8  **pdata,
                       size_t    *pnbytes)
{
char     *fname;
l_int32   i, npages, ret;
L_BYTEA  *bas;
L_PTRA   *pa_data;  /* input pdf data for each page */

    if (!pdata)
        return ERROR_INT("&data not defined", __func__, 1);
    *pdata = NULL;
    if (!pnbytes)
        return ERROR_INT("&nbytes not defined", __func__, 1);
    *pnbytes = 0;
    if (!sa)
        return ERROR_INT("sa not defined", __func__, 1);

        /* Read the pdf files into memory */
    if ((npages = sarrayGetCount(sa)) == 0)
        return ERROR_INT("no filenames found", __func__, 1);
    pa_data = ptraCreate(npages);
    for (i = 0; i < npages; i++) {
        fname = sarrayGetString(sa, i, L_NOCOPY);
        bas = l_byteaInitFromFile(fname);
        ptraAdd(pa_data, bas);
    }

    ret = ptraConcatenatePdfToData(pa_data, sa, pdata, pnbytes);

        /* Cleanup: some pages could have been removed */
    ptraGetActualCount(pa_data, &npages);
    for (i = 0; i < npages; i++) {
        bas = (L_BYTEA *)ptraRemove(pa_data, i, L_NO_COMPACTION);
        l_byteaDestroy(&bas);
    }
    ptraDestroy(&pa_data, FALSE, FALSE);
    return ret;
}

/* --------------------------------------------*/
#endif  /* USE_PDFIO */
/* --------------------------------------------*/
