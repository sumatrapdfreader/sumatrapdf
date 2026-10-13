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
 * \file psio1.c
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
 *    | in environ.h.  This will link psio1stub.c                   |
 *    |=============================================================|
 *
 *     This is a PostScript "device driver" for wrapping images
 *     in PostScript.  The images can be rendered by a PostScript
 *     interpreter for viewing, using evince or gv.  They can also be
 *     rasterized for printing, using gs or an embedded interpreter
 *     in a PostScript printer.  And they can be converted to a pdf
 *     using gs (ps2pdf).
 *
 *     Convert specified files to PS
 *          l_int32          convertFilesToPS()
 *          l_int32          sarrayConvertFilesToPS()
 *          l_int32          convertFilesFittedToPS()
 *          l_int32          sarrayConvertFilesFittedToPS()
 *          l_int32          writeImageCompressedToPSFile()
 *
 *     Convert mixed text/image files to PS
 *          l_int32          convertSegmentedPagesToPS()
 *          l_int32          pixWriteSegmentedPageToPS()
 *          l_int32          pixWriteMixedToPS()
 *
 *     Convert any image file to PS for embedding
 *          l_int32          convertToPSEmbed()
 *
 *     Write all images in a pixa out to PS
 *          l_int32          pixaWriteCompressedToPS()
 *          l_int32          pixWriteCompressedToPS()
 *
 *  These PostScript converters are used in three different ways.
 *
 *  (1) For embedding a PS file in a program like TeX.
 *      convertToPSEmbed() handles this for levels 1, 2 and 3 output,
 *      and prog/converttops wraps this in an executable.
 *      converttops is a generalization of Thomas Merz's jpeg2ps wrapper,
 *      in that it works for all types (formats, depth, colormap)
 *      of input images and gives PS output in one of these formats
 *        * level 1 (uncompressed)
 *        * level 2 (compressed ccittg4 or dct)
 *        * level 3 (compressed flate)
 *
 *  (2) For composing a set of pages with any number of images
 *      painted on them, in either level 2 or level 3 formats.
 *
 *  (3) For printing a page image or a set of page images, at a
 *      resolution that optimally fills the page, using
 *      convertFilesFittedToPS().
 *
 *  The top-level calls of utilities in category 2, which can compose
 *  multiple images on a page, and which generate a PostScript file for
 *  printing or display (e.g., conversion to pdf), are:
 *      convertFilesToPS()
 *      convertFilesFittedToPS()
 *      convertSegmentedPagesToPS()
 *
 *  All images are output with page numbers.  Bounding box hints are
 *  more subtle.  They must be included for embeding images in
 *  TeX, for example, and the low-level writers include bounding
 *  box hints by default.  However, these hints should not be included for
 *  multi-page PostScript that is composed of a sequence of images;
 *  consequently, they are not written when calling higher level
 *  functions such as convertFilesToPS(), convertFilesFittedToPS()
 *  and convertSegmentedPagesToPS().  The function l_psWriteBoundingBox()
 *  sets a flag to give low-level control over this.
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

/*-------------------------------------------------------------*
 *                Convert files in a directory to PS           *
 *-------------------------------------------------------------*/
/*
 * \brief   convertFilesToPS()
 *
 * \param[in]  dirin    input directory
 * \param[in]  substr   [optional] substring filter on filenames; can be NULL
 * \param[in]  res      typ. 300 or 600 ppi
 * \param[in]  fileout  output ps file
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This generates a PS file for all image files in a specified
 *          directory that contain the substr pattern to be matched.
 *      (2) Each image is written to a separate page in the output PS file.
 *      (3) All images are written compressed:
 *              * if tiffg4  -->  use ccittg4
 *              * if jpeg    -->  use dct
 *              * all others -->  use flate
 *          If the image is jpeg or tiffg4, we use the existing compressed
 *          strings for the encoding; otherwise, we read the image into
 *          a pix and flate-encode the pieces.
 *      (4) The resolution is often confusing.  It is interpreted
 *          as the resolution of the output display device:  "If the
 *          input image were digitized at 300 ppi, what would it
 *          look like when displayed at res ppi."  So, for example,
 *          if res = 100 ppi, then the display pixels are 3x larger
 *          than the 300 ppi pixels, and the image will be rendered
 *          3x larger.
 *      (5) The size of the PostScript file is independent of the resolution,
 *          because the entire file is encoded.  The res parameter just
 *          tells the PS decomposer how to render the page.  Therefore,
 *          for minimum file size without loss of visual information,
 *          if the output res is less than 300, you should downscale
 *          the image to the output resolution before wrapping in PS.
 *      (6) The "canvas" on which the image is rendered, at the given
 *          output resolution, is a standard page size (8.5 x 11 in).
 * </pre>
 */
l_ok
convertFilesToPS(const char  *dirin,
                 const char  *substr,
                 l_int32      res,
                 const char  *fileout)
{
SARRAY  *sa;

    if (!dirin)
        return ERROR_INT("dirin not defined", __func__, 1);
    if (!fileout)
        return ERROR_INT("fileout not defined", __func__, 1);
    if (res <= 0) {
        L_INFO("setting res to 300 ppi\n", __func__);
        res = 300;
    }
    if (res < 10 || res > 4000)
        L_WARNING("res is typically in the range 300-600 ppi\n", __func__);

        /* Get all filtered and sorted full pathnames. */
    sa = getSortedPathnamesInDirectory(dirin, substr, 0, 0);

        /* Generate the PS file.  Don't use bounding boxes. */
    l_psWriteBoundingBox(FALSE);
    sarrayConvertFilesToPS(sa, res, fileout);
    l_psWriteBoundingBox(TRUE);
    sarrayDestroy(&sa);
    return 0;
}


/*

 * \brief    sarrayConvertFilesToPS()
 *
 * \param[in]  sarray   of full path names
 * \param[in]  res      typ. 300 or 600 ppi
 * \param[in]  fileout  output ps file
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *     (1) See convertFilesToPS()
 * </pre>
 */
l_ok
sarrayConvertFilesToPS(SARRAY      *sa,
                       l_int32      res,
                       const char  *fileout)
{
char    *fname;
l_int32  i, nfiles, index, ret, format;

    if (!sa)
        return ERROR_INT("sa not defined", __func__, 1);
    if (!fileout)
        return ERROR_INT("fileout not defined", __func__, 1);
    if (res <= 0) {
        L_INFO("setting res to 300 ppi\n", __func__);
        res = 300;
    }
    if (res < 10 || res > 4000)
        L_WARNING("res is typically in the range 300-600 ppi\n", __func__);

    nfiles = sarrayGetCount(sa);
    for (i = 0, index = 0; i < nfiles; i++) {
        fname = sarrayGetString(sa, i, L_NOCOPY);
        ret = pixReadHeader(fname, &format, NULL, NULL, NULL, NULL, NULL);
        if (ret) continue;
        if (format == IFF_UNKNOWN)
            continue;

        writeImageCompressedToPSFile(fname, fileout, res, &index);
    }

    return 0;
}


/*
 * \brief   convertFilesFittedToPS()
 *
 * \param[in]  dirin    input directory
 * \param[in]  substr   [optional] substring filter on filenames; can be NULL)
 * \param[in]  xpts     desired size in printer points; use 0 for default
 * \param[in]  ypts     desired size in printer points; use 0 for default
 * \param[in]  fileout  output ps file
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This generates a PS file for all files in a specified directory
 *          that contain the substr pattern to be matched.
 *      (2) Each image is written to a separate page in the output PS file.
 *      (3) All images are written compressed:
 *              * if tiffg4  -->  use ccittg4
 *              * if jpeg    -->  use dct
 *              * all others -->  use flate
 *          If the image is jpeg or tiffg4, we use the existing compressed
 *          strings for the encoding; otherwise, we read the image into
 *          a pix and flate-encode the pieces.
 *      (4) The resolution is internally determined such that the images
 *          are rendered, in at least one direction, at 100% of the given
 *          size in printer points.  Use 0.0 for xpts or ypts to get
 *          the default value, which is 612.0 or 792.0, rsp.
 *      (5) The size of the PostScript file is independent of the resolution,
 *          because the entire file is encoded.  The %xpts and %ypts
 *          parameter tells the PS decomposer how to render the page.
 * </pre>
 */
l_ok
convertFilesFittedToPS(const char  *dirin,
                       const char  *substr,
                       l_float32    xpts,
                       l_float32    ypts,
                       const char  *fileout)
{
SARRAY  *sa;

    if (!dirin)
        return ERROR_INT("dirin not defined", __func__, 1);
    if (!fileout)
        return ERROR_INT("fileout not defined", __func__, 1);
    if (xpts <= 0.0) {
        L_INFO("setting xpts to 612.0 ppi\n", __func__);
        xpts = 612.0;
    }
    if (ypts <= 0.0) {
        L_INFO("setting ypts to 792.0 ppi\n", __func__);
        ypts = 792.0;
    }
    if (xpts < 100.0 || xpts > 2000.0 || ypts < 100.0 || ypts > 2000.0)
        L_WARNING("xpts,ypts are typically in the range 500-800\n", __func__);

        /* Get all filtered and sorted full pathnames. */
    sa = getSortedPathnamesInDirectory(dirin, substr, 0, 0);

        /* Generate the PS file.  Don't use bounding boxes. */
    l_psWriteBoundingBox(FALSE);
    sarrayConvertFilesFittedToPS(sa, xpts, ypts, fileout);
    l_psWriteBoundingBox(TRUE);
    sarrayDestroy(&sa);
    return 0;
}


/*
 * \brief  sarrayConvertFilesFittedToPS()
 *
 * \param[in]  sarray   of full path names
 * \param[in]  xpts     desired size in printer points; use 0 for default
 * \param[in]  ypts     desired size in printer points; use 0 for default
 * \param[in]  fileout  output ps file
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *     (1) See convertFilesFittedToPS()
 * </pre>
 */
l_ok
sarrayConvertFilesFittedToPS(SARRAY      *sa,
                             l_float32    xpts,
                             l_float32    ypts,
                             const char  *fileout)
{
char    *fname;
l_int32  ret, i, w, h, nfiles, index, format, res;

    if (!sa)
        return ERROR_INT("sa not defined", __func__, 1);
    if (!fileout)
        return ERROR_INT("fileout not defined", __func__, 1);
    if (xpts <= 0.0) {
        L_INFO("setting xpts to 612.0\n", __func__);
        xpts = 612.0;
    }
    if (ypts <= 0.0) {
        L_INFO("setting ypts to 792.0\n", __func__);
        ypts = 792.0;
    }
    if (xpts < 100.0 || xpts > 2000.0 || ypts < 100.0 || ypts > 2000.0)
        L_WARNING("xpts,ypts are typically in the range 500-800\n", __func__);

    nfiles = sarrayGetCount(sa);
    for (i = 0, index = 0; i < nfiles; i++) {
        fname = sarrayGetString(sa, i, L_NOCOPY);
        ret = pixReadHeader(fname, &format, &w, &h, NULL, NULL, NULL);
        if (ret) continue;
        if (format == IFF_UNKNOWN)
            continue;

            /* Be sure the entire image is wrapped */
        if (xpts * h < ypts * w)
            res = (l_int32)((l_float32)w * 72.0 / xpts);
        else
            res = (l_int32)((l_float32)h * 72.0 / ypts);

        writeImageCompressedToPSFile(fname, fileout, res, &index);
    }

    return 0;
}


/*
 * \brief   writeImageCompressedToPSFile()
 *
 * \param[in]     filein      input image file
 * \param[in]     fileout     output ps file
 * \param[in]     res         output printer resolution
 * \param[in,out] pindex      index of image in output ps file
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This wraps a single page image in PS.
 *      (2) The input file can be in any format.  It is compressed as follows:
 *             * if in tiffg4  -->  use ccittg4
 *             * if in jpeg    -->  use dct
 *             * all others    -->  use flate
 *      (3) Before the first call, set %index = 0.  %index is incremented
 *          if the page is successfully written.  It is used to decide
 *          whether to write (index == 0) or append (index > 0) to the file.
 * </pre>
 */
l_ok
writeImageCompressedToPSFile(const char  *filein,
                             const char  *fileout,
                             l_int32      res,
                             l_int32     *pindex)
{
const char  *op;
l_int32      format, retval;

    if (!pindex)
        return ERROR_INT("&index not defined", __func__, 1);

    findFileFormat(filein, &format);
    if (format == IFF_UNKNOWN) {
        L_ERROR("format of %s not known\n", __func__, filein);
        return 1;
    }

    op = (*pindex == 0) ? "w" : "a";
    if (format == IFF_JFIF_JPEG) {
        retval = convertJpegToPS(filein, fileout, op, 0, 0,
                                 res, 1.0, *pindex + 1, TRUE);
    } else if (format == IFF_TIFF_G4) {
        retval = convertG4ToPS(filein, fileout, op, 0, 0,
                               res, 1.0, *pindex + 1, FALSE, TRUE);
    } else {  /* all other image formats */
        retval = convertFlateToPS(filein, fileout, op, 0, 0,
                                  res, 1.0, *pindex + 1, TRUE);
    }
    if (retval == 0) (*pindex)++;

    return retval;
}


/*-------------------------------------------------------------*
 *              Convert mixed text/image files to PS           *
 *-------------------------------------------------------------*/
/*
 * \brief  convertSegmentedPagesToPS()
 *
 * \param[in]     pagedir      input page image directory
 * \param[in]     pagestr      [optional] substring filter on page filenames;
 *                             can be NULL
 * \param[in]     page_numpre  number of characters in page name before number
 * \param[in]     maskdir      input mask image directory
 * \param[in]     maskstr      [optional] substring filter on mask filenames;
 *                             can be NULL
 * \param[in]     mask_numpre  number of characters in mask name before number
 * \param[in]     numpost      number of characters in names after number
 * \param[in]     maxnum       only consider page numbers up to this value
 * \param[in]     textscale    scale of text output relative to pixs
 * \param[in]     imagescale   scale of image output relative to pixs
 * \param[in]     threshold    for binarization; typ. about 190; 0 for default
 * \param[in]     fileout      output ps file
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This generates a PS file for all page image and mask files in two
 *          specified directories and that contain the page numbers as
 *          specified below.  The two directories can be the same, in which
 *          case the page and mask files are differentiated by the two
 *          substrings for string matches.
 *      (2) The page images are taken in lexicographic order.
 *          Mask images whose numbers match the page images are used to
 *          segment the page images.  Page images without a matching
 *          mask image are scaled, thresholded and rendered entirely as text.
 *      (3) Each PS page is generated as a compressed representation of
 *          the page image, where the part of the image under the mask
 *          is suitably scaled and compressed as DCT (i.e., jpeg), and
 *          the remaining part of the page is suitably scaled, thresholded,
 *          compressed as G4 (i.e., tiff g4), and rendered by painting
 *          black through the resulting text mask.
 *      (4) The scaling is typically 2x down for the DCT component
 *          (%imagescale = 0.5) and 2x up for the G4 component
 *          (%textscale = 2.0).
 *      (5) The resolution is automatically set to fit to a
 *          letter-size (8.5 x 11 inch) page.
 *      (6) Both the DCT and the G4 encoding are PostScript level 2.
 *      (7) It is assumed that the page number is contained within
 *          the basename (the filename without directory or extension).
 *          %page_numpre is the number of characters in the page basename
 *          preceding the actual page number; %mask_numpre is likewise for
 *          the mask basename; %numpost is the number of characters
 *          following the page number.  For example, for mask name
 *          mask_006.tif, mask_numpre = 5 ("mask_).
 *      (8) To render a page as is -- that is, with no thresholding
 *          of any pixels -- use a mask in the mask directory that is
 *          full size with all pixels set to 1.  If the page is 1 bpp,
 *          it is not necessary to have a mask.
 * </pre>
 */
l_ok
convertSegmentedPagesToPS(const char  *pagedir,
                          const char  *pagestr,
                          l_int32      page_numpre,
                          const char  *maskdir,
                          const char  *maskstr,
                          l_int32      mask_numpre,
                          l_int32      numpost,
                          l_int32      maxnum,
                          l_float32    textscale,
                          l_float32    imagescale,
                          l_int32      threshold,
                          const char  *fileout)
{
l_int32  pageno, i, npages;
PIX     *pixs, *pixm;
SARRAY  *sapage, *samask;

    if (!pagedir)
        return ERROR_INT("pagedir not defined", __func__, 1);
    if (!maskdir)
        return ERROR_INT("maskdir not defined", __func__, 1);
    if (!fileout)
        return ERROR_INT("fileout not defined", __func__, 1);
    if (threshold <= 0) {
        L_INFO("setting threshold to 190\n", __func__);
        threshold = 190;
    }

        /* Get numbered full pathnames; max size of sarray is maxnum */
    sapage = getNumberedPathnamesInDirectory(pagedir, pagestr,
                                             page_numpre, numpost, maxnum);
    samask = getNumberedPathnamesInDirectory(maskdir, maskstr,
                                             mask_numpre, numpost, maxnum);
    sarrayPadToSameSize(sapage, samask, "");
    if ((npages = sarrayGetCount(sapage)) == 0) {
        sarrayDestroy(&sapage);
        sarrayDestroy(&samask);
        return ERROR_INT("no matching pages found", __func__, 1);
    }

        /* Generate the PS file */
    pageno = 1;
    for (i = 0; i < npages; i++) {
        if ((pixs = pixReadIndexed(sapage, i)) == NULL)
            continue;
        pixm = pixReadIndexed(samask, i);
        pixWriteSegmentedPageToPS(pixs, pixm, textscale, imagescale,
                                  threshold, pageno, fileout);
        pixDestroy(&pixs);
        pixDestroy(&pixm);
        pageno++;
    }

    sarrayDestroy(&sapage);
    sarrayDestroy(&samask);
    return 0;
}


/*
 * \brief   pixWriteSegmentedPageToPS()
 *
 * \param[in]     pixs      all depths; colormap ok
 * \param[in]     pixm      [optional] 1 bpp segmentation mask over image region
 * \param[in]     textscale   scale of text output relative to pixs
 * \param[in]     imagescale  scale of image output relative to pixs
 * \param[in]     threshold   for binarization; typ. about 190; 0 for default
 * \param[in]     pageno    page number in set; use 1 for new output file
 * \param[in]     fileout   output ps file
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This generates the PS string for a mixed text/image page,
 *          and adds it to an existing file if %pageno > 1.
 *          The PS output is determined by fitting the result to
 *          a letter-size (8.5 x 11 inch) page.
 *      (2) The two images (pixs and pixm) are at the same resolution
 *          (typically 300 ppi).  They are used to generate two compressed
 *          images, pixb and pixc, that are put directly into the output
 *          PS file.
 *      (3) pixb is the text component.  In the PostScript world, we think of
 *          it as a mask through which we paint black.  It is produced by
 *          scaling pixs by %textscale, and thresholding to 1 bpp.
 *      (4) pixc is the image component, which is that part of pixs under
 *          the mask pixm.  It is scaled from pixs by %imagescale.
 *      (5) Typical values are textscale = 2.0 and imagescale = 0.5.
 *      (6) If pixm == NULL, the page has only text.  If it is all black,
 *          the page is all image and has no text.
 *      (7) This can be used to write a multi-page PS file, by using
 *          sequential page numbers with the same output file.  It can
 *          also be used to write separate PS files for each page,
 *          by using different output files with %pageno = 0 or 1.
 * </pre>
 */
l_ok
pixWriteSegmentedPageToPS(PIX         *pixs,
                          PIX         *pixm,
                          l_float32    textscale,
                          l_float32    imagescale,
                          l_int32      threshold,
                          l_int32      pageno,
                          const char  *fileout)
{
l_int32    alltext, notext, d, ret;
l_uint32   val;
l_float32  scaleratio;
PIX       *pixmi, *pixmis, *pixt, *pixg, *pixsc, *pixb, *pixc;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (!fileout)
        return ERROR_INT("fileout not defined", __func__, 1);
    if (imagescale <= 0.0 || textscale <= 0.0)
        return ERROR_INT("relative scales must be > 0.0", __func__, 1);

        /* Analyze the page.  Determine the ratio by which the
         * binary text mask is scaled relative to the image part.
         * If there is no image region (alltext == TRUE), the
         * text mask will be rendered directly to fit the page,
         * and scaleratio = 1.0.  */
    alltext = TRUE;
    notext = FALSE;
    scaleratio = 1.0;
    if (pixm) {
        pixZero(pixm, &alltext);  /* pixm empty: all text */
        if (alltext) {
            pixm = NULL;  /* treat it as not existing here */
        } else {
            pixmi = pixInvert(NULL, pixm);
            pixZero(pixmi, &notext);  /* pixm full; no text */
            pixDestroy(&pixmi);
            scaleratio = textscale / imagescale;
        }
    }

    if (pixGetDepth(pixs) == 1) {  /* render tiff g4 */
        pixb = pixClone(pixs);
        pixc = NULL;
    } else {
        pixt = pixConvertTo8Or32(pixs, L_CLONE, 0);  /* clone if possible */

            /* Get the binary text mask.  Note that pixg cannot be a
             * clone of pixs, because it may be altered by pixSetMasked(). */
        pixb = NULL;
        if (notext == FALSE) {
            d = pixGetDepth(pixt);
            if (d == 8)
                pixg = pixCopy(NULL, pixt);
            else  /* d == 32 */
                pixg = pixConvertRGBToLuminance(pixt);
            if (pixm)  /* clear out the image parts */
                pixSetMasked(pixg, pixm, 255);
            if (textscale == 1.0)
                pixsc = pixClone(pixg);
            else if (textscale >= 0.7)
                pixsc = pixScaleGrayLI(pixg, textscale, textscale);
            else
                pixsc = pixScaleAreaMap(pixg, textscale, textscale);
            pixb = pixThresholdToBinary(pixsc, threshold);
            pixDestroy(&pixg);
            pixDestroy(&pixsc);
        }

            /* Get the scaled image region */
        pixc = NULL;
        if (pixm) {
            if (imagescale == 1.0)
                pixsc = pixClone(pixt);  /* can possibly be a clone of pixs */
            else
                pixsc = pixScale(pixt, imagescale, imagescale);

                /* If pixm is not full, clear the pixels in pixsc
                 * corresponding to bg in pixm, where there can be text
                 * that is written through the mask pixb.  Note that
                 * we could skip this and use pixsc directly in
                 * pixWriteMixedToPS(); however, clearing these
                 * non-image regions to a white background will reduce
                 * the size of pixc (relative to pixsc), and hence
                 * reduce the size of the PS file that is generated.
                 * Use a copy so that we don't accidentally alter pixs.  */
            if (notext == FALSE) {
                pixmis = pixScale(pixm, imagescale, imagescale);
                pixmi = pixInvert(NULL, pixmis);
                val = (d == 8) ? 0xff : 0xffffff00;
                pixc = pixCopy(NULL, pixsc);
                pixSetMasked(pixc, pixmi, val);  /* clear non-image part */
                pixDestroy(&pixmis);
                pixDestroy(&pixmi);
            } else {
                pixc = pixClone(pixsc);
            }
            pixDestroy(&pixsc);
        }
        pixDestroy(&pixt);
    }

        /* Generate the PS file.  Don't use bounding boxes. */
    l_psWriteBoundingBox(FALSE);
    ret = pixWriteMixedToPS(pixb, pixc, scaleratio, pageno, fileout);
    l_psWriteBoundingBox(TRUE);
    pixDestroy(&pixb);
    pixDestroy(&pixc);
    return ret;
}


/*
 * \brief  pixWriteMixedToPS()
 *
 * \param[in]     pixb      [optional] 1 bpp mask; typically for text
 * \param[in]     pixc      [optional] 8 or 32 bpp image regions
 * \param[in]     scale     scale factor for rendering pixb, relative to pixc;
 *                          typ. 4.0
 * \param[in]     pageno    page number in set; use 1 for new output file
 * \param[in]     fileout   output ps file
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This low level function generates the PS string for a mixed
 *          text/image page, and adds it to an existing file if
 *          %pageno > 1.
 *      (2) The two images (pixb and pixc) are typically generated at the
 *          resolution that they will be rendered in the PS file.
 *      (3) pixb is the text component.  In the PostScript world, we think of
 *          it as a mask through which we paint black.
 *      (4) pixc is the (typically halftone) image component.  It is
 *          white in the rest of the page.  To minimize the size of the
 *          PS file, it should be rendered at a resolution that is at
 *          least equal to its actual resolution.
 *      (5) %scale gives the ratio of resolution of pixb to pixc.
 *          Typical resolutions are: 600 ppi for pixb, 150 ppi for pixc;
 *          so %scale = 4.0.  If one of the images is not defined,
 *          the value of %scale is ignored.
 *      (6) We write pixc with DCT compression (jpeg).  This is followed
 *          by painting the text as black through the mask pixb.  If
 *          pixc doesn't exist (alltext), we write the text with the
 *          PS "image" operator instead of the "imagemask" operator,
 *          because ghostscript's ps2pdf is flaky when the latter is used.
 *      (7) The actual output resolution is determined by fitting the
 *          result to a letter-size (8.5 x 11 inch) page.
 * <pre>
 */
l_ok
pixWriteMixedToPS(PIX         *pixb,
                  PIX         *pixc,
                  l_float32    scale,
                  l_int32      pageno,
                  const char  *fileout)
{
char        *tname;
const char  *op;
l_int32      resb, resc, endpage, maskop, ret;

    if (!pixb && !pixc)
        return ERROR_INT("pixb and pixc both undefined", __func__, 1);
    if (!fileout)
        return ERROR_INT("fileout not defined", __func__, 1);

        /* Compute the resolution that fills a letter-size page. */
    if (!pixc) {
       resb = getResLetterPage(pixGetWidth(pixb), pixGetHeight(pixb), 0);
    } else {
       resc = getResLetterPage(pixGetWidth(pixc), pixGetHeight(pixc), 0);
       if (pixb)
           resb = (l_int32)(scale * resc);
    }

        /* Write the jpeg image first */
    if (pixc) {
        tname = l_makeTempFilename();
        pixWrite(tname, pixc, IFF_JFIF_JPEG);
        endpage = (pixb) ? FALSE : TRUE;
        op = (pageno <= 1) ? "w" : "a";
        ret = convertJpegToPS(tname, fileout, op, 0, 0, resc, 1.0,
                              pageno, endpage);
        lept_rmfile(tname);
        LEPT_FREE(tname);
        if (ret)
            return ERROR_INT("jpeg data not written", __func__, 1);
    }

        /* Write the binary data, either directly or, if there is
         * a jpeg image on the page, through the mask. */
    if (pixb) {
        tname = l_makeTempFilename();
        pixWrite(tname, pixb, IFF_TIFF_G4);
        op = (pageno <= 1 && !pixc) ? "w" : "a";
        maskop = (pixc) ? 1 : 0;
        ret = convertG4ToPS(tname, fileout, op, 0, 0, resb, 1.0,
                            pageno, maskop, 1);
        lept_rmfile(tname);
        LEPT_FREE(tname);
        if (ret)
            return ERROR_INT("tiff data not written", __func__, 1);
    }

    return 0;
}


/*-------------------------------------------------------------*
 *            Convert any image file to PS for embedding       *
 *-------------------------------------------------------------*/
/*
 * \brief  convertToPSEmbed()
 *
 * \param[in]     filein    input image file, any format
 * \param[in]     fileout   output ps file
 * \param[in]     level     PostScript compression: 1 (uncompressed), 2 or 3
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is a wrapper function that generates a PS file with
 *          a bounding box, from any input image file.
 *      (2) Do the best job of compression given the specified level.
 *          %level=3 does flate compression on anything that is not
 *          tiffg4 (1 bpp), jpeg (8 bpp or rgb), or webp.
 *      (3) If %level=2 and the file is not tiffg4 or jpeg, it will
 *          first be written to file as jpeg with quality = 75.
 *          This will remove the colormap and cause some degradation
 *          in the image.
 *      (4) The bounding box is required when a program such as TeX
 *          (through epsf) places and rescales the image.  It is
 *          sized for fitting the image to an 8.5 x 11.0 inch page.
 * </pre>
 */
l_ok
convertToPSEmbed(const char  *filein,
                 const char  *fileout,
                 l_int32      level)
{
char    *tname, *tmpfile, *basename;
l_int32  d, format;
PIX     *pix, *pixs;

    if (!filein)
        return ERROR_INT("filein not defined", __func__, 1);
    if (!fileout)
        return ERROR_INT("fileout not defined", __func__, 1);
    if (level != 1 && level != 2 && level != 3) {
        L_ERROR("invalid level specified; using level 2\n", __func__);
        level = 2;
    }

    if (level == 1) {  /* no compression */
        pixWritePSEmbed(filein, fileout);
        return 0;
    }

        /* Find the format and write out directly if in jpeg or tiff g4 */
    findFileFormat(filein, &format);
    if (format == IFF_JFIF_JPEG) {
        convertJpegToPSEmbed(filein, fileout);
        return 0;
    } else if (format == IFF_TIFF_G4) {
        convertG4ToPSEmbed(filein, fileout);
        return 0;
    } else if (format == IFF_UNKNOWN) {
        L_ERROR("format of %s not known\n", __func__, filein);
        return 1;
    }

        /* If level 3 and in webp format, convert to jpeg
         * and use dct encoding */
    if (level == 3 && format == IFF_WEBP) {
        pix = pixRead(filein);
        splitPathAtExtension(filein, &basename, NULL);
        tmpfile = stringJoin(basename, ".tmpjpg");
        pixWrite(tmpfile, pix, IFF_JFIF_JPEG);
        convertJpegToPSEmbed(tmpfile, fileout);
        pixDestroy(&pix);
        lept_rmfile(tmpfile);
        LEPT_FREE(tmpfile);
        LEPT_FREE(basename);
        return 0;
    }

        /* If level 3, flate encode. */
    if (level == 3) {
        convertFlateToPSEmbed(filein, fileout);
        return 0;
    }

        /* OK, it's level 2, so we must convert to jpeg or tiff g4 */
    if ((pixs = pixRead(filein)) == NULL)
        return ERROR_INT("image not read from file", __func__, 1);
    d = pixGetDepth(pixs);
    if ((d == 2 || d == 4) && !pixGetColormap(pixs))
        pix = pixConvertTo8(pixs, 0);
    else if (d == 16)
        pix = pixConvert16To8(pixs, L_MS_BYTE);
    else
        pix = pixRemoveColormap(pixs, REMOVE_CMAP_BASED_ON_SRC);
    pixDestroy(&pixs);
    if (!pix)
        return ERROR_INT("converted pix not made", __func__, 1);

    d = pixGetDepth(pix);
    tname = l_makeTempFilename();
    if (d == 1) {
        if (pixWrite(tname, pix, IFF_TIFF_G4)) {
            LEPT_FREE(tname);
            pixDestroy(&pix);
            return ERROR_INT("g4 tiff not written", __func__, 1);
        }
        convertG4ToPSEmbed(tname, fileout);
    } else {
        if (pixWrite(tname, pix, IFF_JFIF_JPEG)) {
            LEPT_FREE(tname);
            pixDestroy(&pix);
            return ERROR_INT("jpeg not written", __func__, 1);
        }
        convertJpegToPSEmbed(tname, fileout);
    }

    lept_rmfile(tname);
    LEPT_FREE(tname);
    pixDestroy(&pix);
    return 0;
}


/*-------------------------------------------------------------*
 *              Write all images in a pixa out to PS           *
 *-------------------------------------------------------------*/
/*
 * \brief  pixaWriteCompressedToPS()
 *
 * \param[in]     pixa      any set of images
 * \param[in]     fileout   output ps file
 * \param[in]     res       resolution for the set of input images
 * \param[in]     level     PostScript compression capability: 2 or 3
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This generates a PostScript file of multiple page images,
 *          all with bounding boxes.
 *      (2) See pixWriteCompressedToPS() for details.
 *      (3) To generate a pdf from %fileout, use:
 *             ps2pdf <infile.ps> <outfile.pdf>
 * </pre>
 */
l_ok
pixaWriteCompressedToPS(PIXA        *pixa,
                        const char  *fileout,
                        l_int32      res,
                        l_int32      level)
{
l_int32  i, n, index, ret;
PIX     *pix;

    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);
    if (!fileout)
        return ERROR_INT("fileout not defined", __func__, 1);
    if (level != 2 && level != 3) {
        L_ERROR("only levels 2 and 3 permitted; using level 2\n", __func__);
        level = 2;
    }

    index = 0;
    n = pixaGetCount(pixa);
    for (i = 0; i < n; i++) {
        pix = pixaGetPix(pixa, i, L_CLONE);
        ret = pixWriteCompressedToPS(pix, fileout, res, level, &index);
        if (ret) L_ERROR("PS string not written for image %d\n", __func__, i);
        pixDestroy(&pix);
    }
    return 0;
}


/*
 * \brief  pixWriteCompressedToPS()
 *
 * \param[in]      pix        any depth; colormap OK
 * \param[in]      fileout    output ps file
 * \param[in]      res        of input image
 * \param[in]      level      PostScript compression capability: 2 or 3
 * \param[in,out]  pindex     index of image in output ps file
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This generates a PostScript string for %pix, and writes it
 *          to a file, with a bounding box.
 *      (2) *pindex keeps track of the number of images that have been
 *          written to %fileout.  If this is the first image to be
 *          converted, set *pindex == 0 before passing it in.  If the
 *          PostScript string is successfully generated, this will increment
 *          *pindex.  If *pindex > 0, the PostScript string will be
 *          appended to %fileout.
 *      (3) PostScript level 2 enables lossless tiffg4 and lossy jpeg
 *          compression.  Level 3 adds lossless flate (essentially gzip)
 *          compression.
 *          * For images with a colormap, lossless flate is often better in
 *            both quality and size than jpeg.
 *          * The decision for images without a colormap affects compression
 *            efficiency: %level2 (jpeg) is usually better than %level3 (flate)
 *          * Because jpeg does not handle 16 bpp, if %level == 2, the image
 *            is converted to 8 bpp (using MSB) and compressed with jpeg,
 *              cmap + level2:        jpeg
 *              cmap + level3:        flate
 *              1 bpp:                tiffg4
 *              2 or 4 bpp + level2:  jpeg
 *              2 or 4 bpp + level3:  flate
 *              8 bpp + level2:       jpeg
 *              8 bpp + level3:       flate
 *              16 bpp + level2:      jpeg   [converted to 8 bpp, with warning]
 *              16 bpp + level3:      flate
 *              32 bpp + level2:      jpeg
 *              32 bpp + level3:      flate
 * </pre>
 */
l_ok
pixWriteCompressedToPS(PIX         *pix,
                       const char  *fileout,
                       l_int32      res,
                       l_int32      level,
                       l_int32     *pindex)
{
char     *tname;
l_int32   writeout, d;
PIX      *pixt;
PIXCMAP  *cmap;

    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    if (!fileout)
        return ERROR_INT("fileout not defined", __func__, 1);
    if (level != 2 && level != 3) {
        L_ERROR("only levels 2 and 3 permitted; using level 2\n", __func__);
        level = 2;
    }
    if (!pindex)
        return ERROR_INT("&index not defined", __func__, 1);

    tname = l_makeTempFilename();
    writeout = TRUE;
    d = pixGetDepth(pix);
    cmap = pixGetColormap(pix);
    if (d == 1) {
        if (pixWrite(tname, pix, IFF_TIFF_G4))
            writeout = FALSE;
    } else if (level == 3) {
        if (pixWrite(tname, pix, IFF_PNG))
            writeout = FALSE;
    } else {  /* level == 2 */
        if (cmap) {
            pixt = pixConvertForPSWrap(pix);
            if (pixWrite(tname, pixt, IFF_JFIF_JPEG))
                writeout = FALSE;
            pixDestroy(&pixt);
        } else if (d == 16) {
            L_WARNING("d = 16; converting to 8 bpp for jpeg\n", __func__);
            pixt = pixConvert16To8(pix, L_MS_BYTE);
            if (pixWrite(tname, pixt, IFF_JFIF_JPEG))
                writeout = FALSE;
            pixDestroy(&pixt);
        } else if (d == 2 || d == 4) {
            pixt = pixConvertTo8(pix, 0);
            if (pixWrite(tname, pixt, IFF_JFIF_JPEG))
                writeout = FALSE;
            pixDestroy(&pixt);
        } else if (d == 8 || d == 32) {
            if (pixWrite(tname, pix, IFF_JFIF_JPEG))
                writeout = FALSE;
        } else {  /* shouldn't happen */
            L_ERROR("invalid depth with level 2: %d\n", __func__, d);
            writeout = FALSE;
        }
    }

    if (writeout)
        writeImageCompressedToPSFile(tname, fileout, res, pindex);

    if (lept_rmfile(tname) != 0)
        L_ERROR("temp file %s was not deleted\n", __func__, tname);
    LEPT_FREE(tname);
    return (writeout) ? 0 : 1;
}

/* --------------------------------------------*/
#endif  /* USE_PSIO */
/* --------------------------------------------*/
