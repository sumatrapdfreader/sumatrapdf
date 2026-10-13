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
 * \file  pageseg.c
 * <pre>
 *
 *      Top level page segmentation
 *          l_int32   pixGetRegionsBinary()
 *
 *      Halftone region extraction
 *          PIX      *pixGenHalftoneMask()    **Deprecated wrapper**
 *          PIX      *pixGenerateHalftoneMask()
 *
 *      Textline extraction
 *          PIX      *pixGenTextlineMask()
 *
 *      Textblock extraction
 *          PIX      *pixGenTextblockMask()
 *
 *      Location and extraction of page foreground; cleaning pages
 *          PIX            *pixCropImage()
 *          static l_int32  pixMaxCompAfterVClosing()
 *          static l_int32  pixFindPageInsideBlackBorder()
 *          static PIX     *pixRescaleForCropping()
 *          PIX            *pixCleanImage()
 *          BOX            *pixFindPageForeground()
 *
 *      Extraction of characters from image with only text
 *          l_int32   pixSplitIntoCharacters()
 *          BOXA     *pixSplitComponentWithProfile()
 *
 *      Extraction of lines of text
 *          PIXA     *pixExtractTextlines()
 *          PIXA     *pixExtractRawTextlines()
 *
 *      How many text columns
 *          l_int32   pixCountTextColumns()
 *
 *      Decision: text vs photo
 *          l_int32   pixDecideIfText()
 *          l_int32   pixFindThreshFgExtent()
 *
 *      Decision: table vs text
 *          l_int32   pixDecideIfTable()
 *          Pix      *pixPrepare1bpp()
 *
 *      Estimate the grayscale background value
 *          l_int32   pixEstimateBackground()
 *
 *      Largest white or black rectangles in an image
 *          l_int32   pixFindLargeRectangles()
 *          l_int32   pixFindLargestRectangle()
 *
 *      Generate rectangle inside connected component
 *          BOX      *pixFindRectangleInCC()
 *
 *      Automatic photoinvert for OCR
 *          PIX      *pixAutoPhotoinvert()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <math.h>
#include "allheaders.h"
#include "pix_internal.h"

    /* These functions are not intended to work on very low-res images */
static const l_int32  MinWidth = 100;
static const l_int32  MinHeight = 100;

static l_ok pixMaxCompAfterVClosing(PIX *pixs, BOX **pbox);
static l_ok pixFindPageInsideBlackBorder(PIX *pixs, BOX **pbox);
static PIX *pixRescaleForCropping(PIX *pixs, l_int32 w, l_int32 h,
                                  l_int32 lr_border, l_int32 tb_border,
                                  l_float32 maxwiden, PIX **ppixsc);

/*------------------------------------------------------------------*
 *                     Top level page segmentation                  *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixGetRegionsBinary()
 *
 * \param[in]    pixs      1 bpp, assumed to be 300 to 400 ppi
 * \param[out]   ppixhm    [optional] halftone mask
 * \param[out]   ppixtm    [optional] textline mask
 * \param[out]   ppixtb    [optional] textblock mask
 * \param[in]    pixadb    input for collecting debug pix; use NULL to skip
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) It is best to deskew the image before segmenting.
 *      (2) Passing in %pixadb enables debug output.
 * </pre>
 */
l_ok
pixGetRegionsBinary(PIX   *pixs,
                    PIX  **ppixhm,
                    PIX  **ppixtm,
                    PIX  **ppixtb,
                    PIXA  *pixadb)
{
l_int32  w, h, htfound, tlfound;
PIX     *pixr, *pix1, *pix2;
PIX     *pixtext;  /* text pixels only */
PIX     *pixhm2;   /* halftone mask; 2x reduction */
PIX     *pixhm;    /* halftone mask;  */
PIX     *pixtm2;   /* textline mask; 2x reduction */
PIX     *pixtm;    /* textline mask */
PIX     *pixvws;   /* vertical white space mask */
PIX     *pixtb2;   /* textblock mask; 2x reduction */
PIX     *pixtbf2;  /* textblock mask; 2x reduction; small comps filtered */
PIX     *pixtb;    /* textblock mask */

    if (ppixhm) *ppixhm = NULL;
    if (ppixtm) *ppixtm = NULL;
    if (ppixtb) *ppixtb = NULL;
    if (!pixs || pixGetDepth(pixs) != 1)
        return ERROR_INT("pixs undefined or not 1 bpp", __func__, 1);
    pixGetDimensions(pixs, &w, &h, NULL);
    if (w < MinWidth || h < MinHeight) {
        L_ERROR("pix too small: w = %d, h = %d\n", __func__, w, h);
        return 1;
    }

        /* 2x reduce, to 150 -200 ppi */
    pixr = pixReduceRankBinaryCascade(pixs, 1, 0, 0, 0);
    if (pixadb) pixaAddPix(pixadb, pixr, L_COPY);

        /* Get the halftone mask */
    pixhm2 = pixGenerateHalftoneMask(pixr, &pixtext, &htfound, pixadb);

        /* Get the textline mask from the text pixels */
    pixtm2 = pixGenTextlineMask(pixtext, &pixvws, &tlfound, pixadb);

        /* Get the textblock mask from the textline mask */
    pixtb2 = pixGenTextblockMask(pixtm2, pixvws, pixadb);
    pixDestroy(&pixr);
    pixDestroy(&pixtext);
    pixDestroy(&pixvws);

        /* Remove small components from the mask, where a small
         * component is defined as one with both width and height < 60 */
    pixtbf2 = NULL;
    if (pixtb2) {
        pixtbf2 = pixSelectBySize(pixtb2, 60, 60, 4, L_SELECT_IF_EITHER,
                                  L_SELECT_IF_GTE, NULL);
        pixDestroy(&pixtb2);
        if (pixadb) pixaAddPix(pixadb, pixtbf2, L_COPY);
    }

        /* Expand all masks to full resolution, and do filling or
         * small dilations for better coverage. */
    pixhm = pixExpandReplicate(pixhm2, 2);
    pix1 = pixSeedfillBinary(NULL, pixhm, pixs, 8);
    pixOr(pixhm, pixhm, pix1);
    pixDestroy(&pixhm2);
    pixDestroy(&pix1);
    if (pixadb) pixaAddPix(pixadb, pixhm, L_COPY);

    pix1 = pixExpandReplicate(pixtm2, 2);
    pixtm = pixDilateBrick(NULL, pix1, 3, 3);
    pixDestroy(&pixtm2);
    pixDestroy(&pix1);
    if (pixadb) pixaAddPix(pixadb, pixtm, L_COPY);

    if (pixtbf2) {
        pix1 = pixExpandReplicate(pixtbf2, 2);
        pixtb = pixDilateBrick(NULL, pix1, 3, 3);
        pixDestroy(&pixtbf2);
        pixDestroy(&pix1);
        if (pixadb) pixaAddPix(pixadb, pixtb, L_COPY);
    } else {
        pixtb = pixCreateTemplate(pixs);  /* empty mask */
    }

        /* Debug: identify objects that are neither text nor halftone image */
    if (pixadb) {
        pix1 = pixSubtract(NULL, pixs, pixtm);  /* remove text pixels */
        pix2 = pixSubtract(NULL, pix1, pixhm);  /* remove halftone pixels */
        pixaAddPix(pixadb, pix2, L_INSERT);
        pixDestroy(&pix1);
    }

        /* Debug: display textline components with random colors */
    if (pixadb) {
        l_int32  w, h;
        BOXA    *boxa;
        PIXA    *pixa;
        boxa = pixConnComp(pixtm, &pixa, 8);
        pixGetDimensions(pixtm, &w, &h, NULL);
        pix1 = pixaDisplayRandomCmap(pixa, w, h);
        pixcmapResetColor(pixGetColormap(pix1), 0, 255, 255, 255);
        pixaAddPix(pixadb, pix1, L_INSERT);
        pixaDestroy(&pixa);
        boxaDestroy(&boxa);
    }

        /* Debug: identify the outlines of each textblock */
    if (pixadb) {
        PIXCMAP  *cmap;
        PTAA     *ptaa;
        ptaa = pixGetOuterBordersPtaa(pixtb);
        lept_mkdir("lept/pageseg");
        ptaaWriteDebug("/tmp/lept/pageseg/tb_outlines.ptaa", ptaa, 1);
        pix1 = pixRenderRandomCmapPtaa(pixtb, ptaa, 1, 16, 1);
        cmap = pixGetColormap(pix1);
        pixcmapResetColor(cmap, 0, 130, 130, 130);
        pixaAddPix(pixadb, pix1, L_INSERT);
        ptaaDestroy(&ptaa);
    }

        /* Debug: get b.b. for all mask components */
    if (pixadb) {
        BOXA  *bahm, *batm, *batb;
        bahm = pixConnComp(pixhm, NULL, 4);
        batm = pixConnComp(pixtm, NULL, 4);
        batb = pixConnComp(pixtb, NULL, 4);
        boxaWriteDebug("/tmp/lept/pageseg/htmask.boxa", bahm);
        boxaWriteDebug("/tmp/lept/pageseg/textmask.boxa", batm);
        boxaWriteDebug("/tmp/lept/pageseg/textblock.boxa", batb);
        boxaDestroy(&bahm);
        boxaDestroy(&batm);
        boxaDestroy(&batb);
    }
    if (pixadb) {
        pixaConvertToPdf(pixadb, 0, 1.0, 0, 0, "Debug page segmentation",
                         "/tmp/lept/pageseg/debug.pdf");
        L_INFO("Writing debug pdf to /tmp/lept/pageseg/debug.pdf\n", __func__);
    }

    if (ppixhm)
        *ppixhm = pixhm;
    else
        pixDestroy(&pixhm);
    if (ppixtm)
        *ppixtm = pixtm;
    else
        pixDestroy(&pixtm);
    if (ppixtb)
        *ppixtb = pixtb;
    else
        pixDestroy(&pixtb);

    return 0;
}


/*------------------------------------------------------------------*
 *                    Halftone region extraction                    *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixGenHalftoneMask()
 *
 * <pre>
 * Deprecated:
 *   This wrapper avoids an ABI change with tesseract 3.0.4.
 *   It should be removed when we no longer need to support 3.0.4.
 *   The debug parameter is ignored (assumed 0).
 * </pre>
 */
PIX *
pixGenHalftoneMask(PIX      *pixs,
                   PIX     **ppixtext,
                   l_int32  *phtfound,
                   l_int32   debug)
{
    return pixGenerateHalftoneMask(pixs, ppixtext, phtfound, NULL);
}


/*!
 * \brief   pixGenerateHalftoneMask()
 *
 * \param[in]    pixs      1 bpp, assumed to be 150 to 200 ppi
 * \param[out]   ppixtext  [optional] text part of pixs
 * \param[out]   phtfound  [optional] 1 if the mask is not empty
 * \param[in]    pixadb    input for collecting debug pix; use NULL to skip
 * \return  pixd halftone mask, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is not intended to work on small thumbnails.  The
 *          dimensions of pixs must be at least MinWidth x MinHeight.
 * </pre>
 */
PIX *
pixGenerateHalftoneMask(PIX      *pixs,
                        PIX     **ppixtext,
                        l_int32  *phtfound,
                        PIXA     *pixadb)
{
l_int32  w, h, empty;
PIX     *pix1, *pix2, *pixhs, *pixhm, *pixd;

    if (ppixtext) *ppixtext = NULL;
    if (phtfound) *phtfound = 0;
    if (!pixs || pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, NULL);
    if (w < MinWidth || h < MinHeight) {
        L_ERROR("pix too small: w = %d, h = %d\n", __func__, w, h);
        return NULL;
    }

        /* Compute seed for halftone parts at 8x reduction */
    pix1 = pixReduceRankBinaryCascade(pixs, 4, 4, 0, 0);
    pix2 = pixOpenBrick(NULL, pix1, 5, 5);
    pixhs = pixExpandReplicate(pix2, 4);  /* back to 2x reduction */
    pixDestroy(&pix1);
    pixDestroy(&pix2);
    if (pixadb) pixaAddPix(pixadb, pixhs, L_COPY);

        /* Compute mask for connected regions */
    pixhm = pixCloseSafeBrick(NULL, pixs, 4, 4);
    if (pixadb) pixaAddPix(pixadb, pixhm, L_COPY);

        /* Fill seed into mask to get halftone mask */
    pixd = pixSeedfillBinary(NULL, pixhs, pixhm, 4);
    if (pixadb) pixaAddPix(pixadb, pixd, L_COPY);

#if 0
    pixOpenBrick(pixd, pixd, 9, 9);
#endif

        /* Check if mask is empty */
    pixZero(pixd, &empty);
    if (phtfound && !empty)
        *phtfound = 1;

        /* Optionally, get all pixels that are not under the halftone mask */
    if (ppixtext) {
        if (empty)
            *ppixtext = pixCopy(NULL, pixs);
        else
            *ppixtext = pixSubtract(NULL, pixs, pixd);
        if (pixadb) pixaAddPix(pixadb, *ppixtext, L_COPY);
    }

    pixDestroy(&pixhs);
    pixDestroy(&pixhm);
    return pixd;
}


/*------------------------------------------------------------------*
 *                         Textline extraction                      *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixGenTextlineMask()
 *
 * \param[in]    pixs      1 bpp, assumed to be 150 to 200 ppi
 * \param[out]   ppixvws   vertical whitespace mask
 * \param[out]   ptlfound  [optional] 1 if the mask is not empty
 * \param[in]    pixadb    input for collecting debug pix; use NULL to skip
 * \return  pixd textline mask, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The input pixs should be deskewed.
 *      (2) pixs should have no halftone pixels.
 *      (3) This is not intended to work on small thumbnails.  The
 *          dimensions of pixs must be at least MinWidth x MinHeight.
 *      (4) Both the input image and the returned textline mask
 *          are at the same resolution.
 * </pre>
 */
PIX *
pixGenTextlineMask(PIX      *pixs,
                   PIX     **ppixvws,
                   l_int32  *ptlfound,
                   PIXA     *pixadb)
{
l_int32  w, h, empty;
PIX     *pix1, *pix2, *pixvws, *pixd;

    if (ptlfound) *ptlfound = 0;
    if (!ppixvws)
        return (PIX *)ERROR_PTR("&pixvws not defined", __func__, NULL);
    *ppixvws = NULL;
    if (!pixs || pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, NULL);
    if (w < MinWidth || h < MinHeight) {
        L_ERROR("pix too small: w = %d, h = %d\n", __func__, w, h);
        return NULL;
    }

        /* First we need a vertical whitespace mask.  Invert the image. */
    pix1 = pixInvert(NULL, pixs);

        /* The whitespace mask will break textlines where there
         * is a large amount of white space below or above.
         * This can be prevented by identifying regions of the
         * inverted image that have large horizontal extent (bigger than
         * the separation between columns) and significant
         * vertical extent (bigger than the separation between
         * textlines), and subtracting this from the bg. */
    pix2 = pixMorphCompSequence(pix1, "o80.60", 0);
    pixSubtract(pix1, pix1, pix2);
    if (pixadb) pixaAddPix(pixadb, pix1, L_COPY);
    pixDestroy(&pix2);

        /* Identify vertical whitespace by opening the remaining bg.
         * o5.1 removes thin vertical bg lines and o1.200 extracts
         * long vertical bg lines. */
    pixvws = pixMorphCompSequence(pix1, "o5.1 + o1.200", 0);
    *ppixvws = pixvws;
    if (pixadb) pixaAddPix(pixadb, pixvws, L_COPY);
    pixDestroy(&pix1);

        /* Three steps to getting text line mask:
         *   (1) close the characters and words in the textlines
         *   (2) open the vertical whitespace corridors back up
         *   (3) small opening to remove noise    */
    pix1 = pixMorphSequence(pixs, "c30.1", 0);
    if (pixadb) pixaAddPix(pixadb, pix1, L_COPY);
    pixd = pixSubtract(NULL, pix1, pixvws);
    pixOpenBrick(pixd, pixd, 3, 3);
    if (pixadb) pixaAddPix(pixadb, pixd, L_COPY);
    pixDestroy(&pix1);

        /* Check if text line mask is empty */
    if (ptlfound) {
        pixZero(pixd, &empty);
        if (!empty)
            *ptlfound = 1;
    }

    return pixd;
}


/*------------------------------------------------------------------*
 *                       Textblock extraction                       *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixGenTextblockMask()
 *
 * \param[in]    pixs     1 bpp, textline mask, assumed to be 150 to 200 ppi
 * \param[in]    pixvws   vertical white space mask
 * \param[in]    pixadb   input for collecting debug pix; use NULL to skip
 * \return  pixd textblock mask, or NULL if empty or on error
 *
 * <pre>
 * Notes:
 *      (1) Both the input masks (textline and vertical white space) and
 *          the returned textblock mask are at the same resolution.
 *      (2) This is not intended to work on small thumbnails.  The
 *          dimensions of pixs must be at least MinWidth x MinHeight.
 *      (3) The result is somewhat noisy, in that small "blocks" of
 *          text may be included.  These can be removed by post-processing,
 *          using, e.g.,
 *             pixSelectBySize(pix, 60, 60, 4, L_SELECT_IF_EITHER,
 *                             L_SELECT_IF_GTE, NULL);
 * </pre>
 */
PIX *
pixGenTextblockMask(PIX   *pixs,
                    PIX   *pixvws,
                    PIXA  *pixadb)
{
l_int32  w, h, empty;
PIX     *pix1, *pix2, *pix3, *pixd;

    if (!pixs || pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, NULL);
    if (w < MinWidth || h < MinHeight) {
        L_ERROR("pix too small: w = %d, h = %d\n", __func__, w, h);
        return NULL;
    }
    if (!pixvws)
        return (PIX *)ERROR_PTR("pixvws not defined", __func__, NULL);

        /* Join pixels vertically to make a textblock mask */
    pix1 = pixMorphSequence(pixs, "c1.10 + o4.1", 0);
    pixZero(pix1, &empty);
    if (empty) {
        pixDestroy(&pix1);
        L_INFO("no fg pixels in textblock mask\n", __func__);
        return NULL;
    }
    if (pixadb) pixaAddPix(pixadb, pix1, L_COPY);

        /* Solidify the textblock mask and remove noise:
         *   (1) For each cc, close the blocks and dilate slightly
         *       to form a solid mask.
         *   (2) Small horizontal closing between components.
         *   (3) Open the white space between columns, again.
         *   (4) Remove small components. */
    pix2 = pixMorphSequenceByComponent(pix1, "c30.30 + d3.3", 8, 0, 0, NULL);
    pixCloseSafeBrick(pix2, pix2, 10, 1);
    if (pixadb) pixaAddPix(pixadb, pix2, L_COPY);
    pix3 = pixSubtract(NULL, pix2, pixvws);
    if (pixadb) pixaAddPix(pixadb, pix3, L_COPY);
    pixd = pixSelectBySize(pix3, 25, 5, 8, L_SELECT_IF_BOTH,
                            L_SELECT_IF_GTE, NULL);
    if (pixadb) pixaAddPix(pixadb, pixd, L_COPY);

    pixDestroy(&pix1);
    pixDestroy(&pix2);
    pixDestroy(&pix3);
    return pixd;
}


/*------------------------------------------------------------------*
 *    Location and extraction of page foreground; cleaning pages    *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixCropImage()
 *
 * \param[in]    pixs        full resolution (any type or depth)
 * \param[in]    lr_clear    full res pixels cleared at left and right sides
 * \param[in]    tb_clear    full res pixels cleared at top and bottom sides
 * \param[in]    edgeclean   parameter for removing edge noise (-1 to 15)
 *                           default = 0 (no removal);
 *                           15 is maximally aggressive for random noise
 *                           -1 for aggressively removing side noise
 *                           -2 to extract page embedded in black background
 * \param[in]    lr_border   full res final "added" pixels on left and right
 * \param[in]    tb_border   full res final "added" pixels on top and bottom
 * \param[in]    maxwiden    max fractional horizontal stretch allowed
 * \param[in]    printwiden  0 to skip, 1 for 8.5x11, 2 for A4
 * \param[in]   *debugfile   [optional] usually is NULL
 * \param[out]  *pcropbox    [optional] crop box at full resolution
 * \return  cropped pix, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This binarizes and crops a page image.
 *          (a) Binarizes if necessary and does 2x reduction.
 *          (b) Clears near the border by %lr_clear and %tb_clear full
 *              resolution pixels.  (This is done at 2x reduction.)
 *          (c) If %edgeclean > 0, it removes isolated sets of pixels,
 *              using a close/open operation of size %edgeclean + 1.
 *              If %edgeclean == -1, it uses a large vertical morphological
 *              close/open and the extraction of either the largest
 *              resulting connected component (or the largest two components
 *              if the page has 2 columns), to eliminate noise on left
 *              and right sides.
 *              If %edgeclean == -2, it extracts the page region from a
 *              possible exterior black surround.
 *          (d) Find the bounding box of remaining fg pixels and scales
 *              the box up 2x back to full resolution.
 *          (e) Crops the binarized image to the bounding box.
 *          (f) Slightly thickens long horizontal lines.
 *          (g) Rescales this image to fit within the original image,
 *              less lr_border on the sides and tb_border above and below.
 *              The rescaling is done isomorphically with a (possible)
 *              optional additional widening.  Suggest the additional
 *              widening factor not exceed 1.15.
 *          (h) Optionally do additional horizontal stretch if needed to
 *              better fill a printed page.  Default is 0 to skip; 1 to
 *              widen for 8.5x11 page, 2 for A4 page.
 *          Note that (b) - (d) are done at 2x reduction for efficiency.
 *      (2) Side clearing must not exceed 1/6 of the dimension on that side.
 *      (3) The clear and border pixel parameters must be >= 0.
 *      (4) The "clear" parameters act on the input image, whereas the
 *          "border" parameters act to give a white border to the final
 *          image.  They are not literally added, because the input and final
 *          images are the same size.  If the resulting images are to be
 *          printed, it is useful to have border pixel parameters of at
 *          least 60 at 300 ppi, to avoid losing content at the edges.
 *      (5) This is not intended to work on small thumbnails.  The
 *          dimensions of pixs must be at least MinWidth x MinHeight.
 *      (6) Step (f) above helps with orthographically-produced music notation,
 *          where the horizontal staff lines can be very thin and thus
 *          subject to printer alias.
 *      (7) With orthographically-produced (as opposed to scanned) images,
 *          there is no scan noise, so you should skip noise removal
 *          by setting %edgeclean = 0.
 *      (8) If you are not concerned with printing on paper, use the
 *          default value 0 for %printwiden.  Widening only takes place
 *          if the ratio h/w exceeds the specified paper size by 3%,
 *          and the horizontal scaling factor will not exceed 1.25.
 * </pre>
 */
PIX *
pixCropImage(PIX         *pixs,
             l_int32      lr_clear,
             l_int32      tb_clear,
             l_int32      edgeclean,
             l_int32      lr_border,
             l_int32      tb_border,
             l_float32    maxwiden,
             l_int32      printwiden,
             const char  *debugfile,
             BOX        **pcropbox)
{
char       cmd[64];
l_int32    w, h, val, ret;
l_float32  r1, r2;
BOX       *box1, *box2;
PIX       *pix1, *pix2, *pix3, *pix4;
PIXA      *pixa1;

    if (pcropbox) *pcropbox = NULL;
    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (edgeclean > 15) {
        L_WARNING("edgeclean > 15; setting to 15\n", __func__);
        edgeclean = 15;
    }
    if (edgeclean < -1) {
        lept_stderr("Using edgeclean = -2\n");
        edgeclean = -2;
    }
    pixGetDimensions(pixs, &w, &h, NULL);
    if (w < MinWidth || h < MinHeight) {
        L_ERROR("pix too small: w = %d, h = %d\n", __func__, w, h);
        return NULL;
    }
    if (lr_clear < 0) lr_clear = 0;
    if (tb_clear < 0) tb_clear = 0;
    if (lr_border < 0) lr_border = 0;
    if (tb_border < 0) tb_border = 0;
    if (lr_clear > w / 6 || tb_clear > h / 6) {
        L_ERROR("lr_clear or tb_clear too large; must be <= %d and %d\n",
                __func__, w / 6, h / 6);
        return NULL;
    }
    if (maxwiden > 1.15)
        L_WARNING("maxwiden = %f > 1.15; suggest between 1.0 and 1.15\n",
                  __func__, maxwiden);
    if (printwiden < 0 || printwiden > 2) printwiden = 0;
    pixa1 = (debugfile) ? pixaCreate(5) : NULL;
    if (pixa1) pixaAddPix(pixa1, pixs, L_COPY);

        /* Binarize if necessary and 2x reduction */
    pix1 = pixBackgroundNormTo1MinMax(pixs, 1, 1);
    pix2 = pixReduceRankBinary2(pix1, 2, NULL);

        /* Clear out pixels near the image edges */
    pixSetOrClearBorder(pix2, lr_clear / 2, lr_clear / 2, tb_clear / 2,
                        tb_clear / 2, PIX_CLR);
    if (pixa1) pixaAddPix(pixa1, pixScale(pix2, 2.0, 2.0), L_INSERT);

        /* Choose one of three methods for extracting foreground pixels:
         * (1) Include all foreground pixels
         * (2) Do a morphological close/open to remove noise throughout
         *     the image before finding a b.b. for remaining f.g. pixels
         * (3) Do a large vertical closing and choose the largest (by area)
         *     component to avoid foreground noise on left and right sides */
    if (edgeclean == 0) {
        ret = pixClipToForeground(pix2, NULL, &box1);
    } else if (edgeclean > 0) {
        val = edgeclean + 1;
        snprintf(cmd, 64, "c%d.%d + o%d.%d", val, val, val, val);
        pix3 = pixMorphSequence(pix2, cmd, 0);
        ret = pixClipToForeground(pix3, NULL, &box1);
        pixDestroy(&pix3);
    } else if (edgeclean == -1) {
        ret = pixMaxCompAfterVClosing(pix2, &box1);
    } else {  /* edgeclean == -2 */
        ret = pixFindPageInsideBlackBorder(pix2, &box1);
    }
    pixDestroy(&pix2);
    if (ret) {
        L_ERROR("no returned b.b. for foreground\n", __func__);
        boxDestroy(&box1);
        pixDestroy(&pix1);
        pixaDestroy(&pixa1);
        return NULL;
    }

        /* Transform to full resolution */
    box2 = boxTransform(box1, 0, 0, 2.0, 2.0);  /* full res */
    boxDestroy(&box1);
    if (pixa1) {
        pix2 = pixCopy(NULL, pix1);
        pixRenderBoxArb(pix2, box2, 5, 255, 0, 0);
        pixaAddPix(pixa1, pix2, L_INSERT);
    }

        /* Grab the foreground region */
    pix2 = pixClipRectangle(pix1, box2, NULL);
    pixDestroy(&pix1);

        /* Slightly thicken long horizontal lines.  This prevents loss of
         * printed thin music staff lines due to aliasing. */
    pix3 = pixMorphSequence(pix2, "o80.1 + d1.2", 0);
    pixOr(pix2, pix2, pix3);
    pixDestroy(&pix3);

        /* Rescale the fg and paste into the input-sized image */
    pix3 = pixRescaleForCropping(pix2,  w, h, lr_border, tb_border,
                                 maxwiden, NULL);
    pixDestroy(&pix2);
    if (pixa1) {
        pix2 = pixCopy(NULL, pix3);
        pixaAddPix(pixa1, pix2, L_INSERT);
    }

        /* Optionally widen image if possible, for printing on 8.5 x 11 inch
         * or A4 paper.  Specifically, widen the image if the h/w asperity
         * ratio of the input image exceeds that of the selected paper by
         * more than 3%.  Do not widen by more than 20%.  */
    r1 = (l_float32)h / (l_float32)w;
    r2 = 0.0;  /* for default case */
    if (printwiden == 1)  /* standard */
        r2 = r1 / 1.294;
    else if (printwiden == 2)  /* A4 */
        r2 = r1 / 1.414;
    if (r2 > 1.03) {
        r2 = L_MIN(r2, 1.20);
        lept_stderr("oversize h/w ratio by factor %6.3f\n", r2);
        pix4 = pixScale(pix3, r2, 1.0);
    } else {
        pix4 = pixClone(pix3);
    }
    pixDestroy(&pix3);

    if (pcropbox)
        *pcropbox = box2;
    else
        boxDestroy(&box2);
    if (pixa1) {
       pixaAddPix(pixa1, pix4, L_COPY);
       lept_stderr("Writing debug file: %s\n", debugfile);
       pixaConvertToPdf(pixa1, 0, 1.0, L_DEFAULT_ENCODE, 0, NULL, debugfile);
       pixaDestroy(&pixa1);
    }
    return pix4;
}


/*!
 * \brief   pixMaxCompAfterVClosing()
 *
 * \param[in]    pixs        1 bpp (input at 2x reduction)
 * \param[out]  **pbox       main region at input resolution (2x reduction)
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This removes foreground noise along left and right edges,
 *          returning a bounding box for the remaining foreground pixels
 *          at the input resolution.
 *      (2) The input %pixs should be at a resolution 100 - 150 ppi.
 *      (3) It does two 2x level1 rank binary reductions, followed
 *          by a large vertical close/open, with a very small horizontal
 *          close/oopen, and then a 4x expansion back to the input resolution.
 *      (4) To work properly with 2-column layout, if the largest and
 *          second-largest regions are comparable in size, both are included.
 *      (5) This is used as an option to pixCropImage(), when given
 *          an %edgecrop parameter of -1.
 * </pre>
 */
static l_ok
pixMaxCompAfterVClosing(PIX   *pixs,
                        BOX  **pbox)
{
l_int32  w1, h1, w2, h2, n, empty;
BOX     *box1, *box2;
BOXA    *boxa1, *boxa2;
PIX     *pix1;

    if (!pbox)
        return ERROR_INT("pbox not defined", __func__, 1);
    *pbox = NULL;
    if (!pixs || pixGetDepth(pixs) != 1)
        return ERROR_INT("pixs undefined or not 1 bpp", __func__, 1);

        /* Strong vertical closing */
    pix1 = pixMorphSequence(pixs, "r11 + c3.80 + o3.80 + x4", 0);
    pixZero(pix1, &empty);
    if (empty) {
        pixDestroy(&pix1);
        return ERROR_INT("pix1 is empty", __func__, 1);
    }

        /* Find the two c.c. with largest area. If they are not comparable
         * in area, return the bounding box of the largest; otherwise,
         * return the bounding box of both regions. */
    boxa1 = pixConnCompBB(pix1, 8);
    pixDestroy(&pix1);
    boxa2 = boxaSort(boxa1, L_SORT_BY_AREA, L_SORT_DECREASING, NULL);
    if ((n = boxaGetCount(boxa2)) == 1) {
        *pbox = boxaGetBox(boxa2, 0, L_COPY);
    } else {  /* 2 or more */
        box1 = boxaGetBox(boxa2, 0, L_COPY);
        box2 = boxaGetBox(boxa2, 1, L_COPY);
        boxGetGeometry(box1, NULL, NULL, &w1, &h1);
        boxGetGeometry(box2, NULL, NULL, &w2, &h2);
        if (((l_float32)(w2 * h2) / (l_float32)(w1 * h1)) > 0.7) {
            *pbox = boxBoundingRegion(box1, box2);
            boxDestroy(&box1);
        } else {
            *pbox = box1;
        }
        boxDestroy(&box2);
    }
    boxaDestroy(&boxa1);
    boxaDestroy(&boxa2);
    return 0; 
}


/*!
 * \brief   pixFindPageInsideBlackBorder()
 *
 * \param[in]    pixs        1 bpp (input at 2x reduction)
 * \param[out]  **pbox       page region at input resolution (2x reduction)
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This extracts the page region from the image, returning a
 *          bounding box for the remaining foreground pixels.  It is designed
 *          to work when the page is within a fairly solid black border.
 *      (2) It returns a bounding box for the page region at the input res.
 *      (3) The input %pixs is expected to be at a resolution 100 - 150 ppi.
 *      (4) This is used as an option to pixCropImage(), when given an
 *          %edgecrop parameter of -2.
 * </pre>
 */
static l_ok
pixFindPageInsideBlackBorder(PIX   *pixs,
                             BOX  **pbox)
{
l_int32  empty, x, y;
BOX     *box1, *box2, *box3;
BOXA    *boxa1, *boxa2;
PIX     *pix1, *pix2, *pix3;

    if (!pbox)
        return ERROR_INT("pbox not defined", __func__, 1);
    *pbox = NULL;
    if (!pixs || pixGetDepth(pixs) != 1)
        return ERROR_INT("pixs undefined or not 1 bpp", __func__, 1);

        /* Reduce 4x and remove some remaining small foreground */
    pix1 = pixMorphSequence(pixs, "r22 + c5.5 + o7.7", 0);
    pixZero(pix1, &empty);
    if (empty) {
        pixDestroy(&pix1);
        return ERROR_INT("pix1 is empty", __func__, 1);
    }

        /* Photoinvert image and Find the c.c. with largest area. */
    pixInvert(pix1, pix1);
    pix2 = pixMorphSequence(pix1, "c11.11 + o11.11", 0);
    pixDestroy(&pix1);
    boxa1 = pixConnCompBB(pix2, 8);
    pixDestroy(&pix2);
    boxa2 = boxaSort(boxa1, L_SORT_BY_AREA, L_SORT_DECREASING, NULL);
    box1 = boxaGetBox(boxa2, 0, L_COPY);  /* largest by area */
    boxAdjustSides(box1, box1, 5, -5, 5, -5);
    box2 = boxTransform(box1, 0, 0, 4.0, 4.0);

        /* Crop this page from the original image and find the foreground */
    pix3 = pixClipRectangle(pixs, box2, NULL);
    pixClipToForeground(pix3, NULL, &box3);
    pixDestroy(&pix3);
    boxGetGeometry(box2, &x, &y, NULL, NULL);
    *pbox = boxTransform(box3, x, y, 1.0, 1.0);
    boxaDestroy(&boxa1);
    boxaDestroy(&boxa2);
    boxDestroy(&box1);
    boxDestroy(&box2);
    boxDestroy(&box3);
    return 0;
}


/*!
 * \brief   pixRescaleForCropping()
 *
 * \param[in]    pixs        1 bpp
 * \param[in]    w           width of output lmage
 * \param[in]    h           height of output lmage
 * \param[in]    lr_border   cleared final border pixels on left and right
 * \param[in]    tb_border   cleared final border pixels on top and bottom
 * \param[in]    maxwiden    max fractional horizontal stretch allowed; >= 1.0
 * \param[out]  *ppixsc      [optional] rescaled foreground region
 * \return  pixd  output image, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This rescales %pixs to fit maximally within an image of
 *          size (w x h), under two conditions:
 *          (a) the final image has cleared border regions given by the
 *              input parameters %lr_border and %tb_border, and
 *          (b) the input image is first isotropically scaled to fit
 *              maximally within the allowed final region, and then further
 *              maxiximally widened, subject to the constraints of the
 *              cleared border and the %maxwiden parameter.
 *      (2) The cleared border pixel parameters must be >= 0.
 *      (3) If there is extra horizontal stretching by a factor
 *          %maxwiden larger than about 1.15, the appearance may be
 *          unpleasingly distorted; hence the suggestion not to exceed it.
 * </pre>
 */
static PIX *
pixRescaleForCropping(PIX       *pixs,
                      l_int32    w,
                      l_int32    h,
                      l_int32    lr_border,
                      l_int32    tb_border,
                      l_float32  maxwiden,
                      PIX      **ppixsc)
{
static l_int32  first_time = TRUE;
l_int32         wi, hi, wmax, hmax, wn, wf, hf, xf;
l_float32       ratio, scaleh, scalew, scalewid;
PIX            *pix1, *pixd;

    if (ppixsc) *ppixsc = NULL;
    if (!pixs || pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    if (lr_border < 0) lr_border = 0;
    if (tb_border < 0) tb_border = 0;
    maxwiden = L_MAX(1.0, maxwiden);
    if (maxwiden > 1.15)
        L_WARNING("maxwiden = %f > 1.15; suggest between 1.0 and 1.15\n",
                  __func__, maxwiden);

        /* Rescale the foreground region.
         * First, decide if scaling is to full width or full height.
         * If scaling to full height, determine how much additional
         * width widening is possible, given the maxwiden constraint.
         * If scaling to full width, both width and height are
         * scaled isotropically.  Scaling is done so that the resulting
         * foreground is maximally widened, so it can be horizontally
         * centered in an image of size (w x h), less %lr_border
         * on each side. */
    pixGetDimensions(pixs, &wi, &hi, NULL);
    wmax = w - 2 * lr_border;
    hmax = h - 2 * tb_border;
    ratio = (l_float32)(wmax * hi) / (l_float32)(hmax * wi);
    if (ratio >= 1.0) {  /* width can be widened after isotropic scaling */
        scaleh = (l_float32)hmax / (l_float32)hi;
        wn = scaleh * wi;  /* scaled but not widened */
        scalewid = L_MIN(maxwiden, (l_float32)wmax / (l_float32)wn);
        scalew = scaleh * scalewid;
        wf = scalew * wi;
        hf = hmax;  /* scale to full height */
        pix1 = pixScale(pixs, scalew, scaleh);
        if (first_time == TRUE) {
            lept_stderr("Width stretched by factor %5.3f\n", scalewid);
            first_time = FALSE;
        }
        xf = (w - wf) / 2.0;
    } else {  /* width cannot be widened after isotropic scaling */
        scalew = (l_float32)wmax / (l_float32)wi;
        pix1 = pixScale(pixs, scalew, scalew);
        wf = wmax;  /* scale to full width */
        hf = scalew * hi;  /* no extra vertical stretching allowed */
        xf = lr_border;
    }

        /* Paste it, horizontally centered and vertically placed as
         * high as allowed (by %tb_border) into the final page image. */
    pixd = pixCreate(w, h, 1);
    pixRasterop(pixd, xf, tb_border, wf, hf, PIX_SRC, pix1, 0, 0);

    if (ppixsc)
        *ppixsc = pix1;
    else
        pixDestroy(&pix1);
    return pixd;
}


/*!
 * \brief   pixCleanImage()
 *
 * \param[in]    pixs        full resolution (any type or depth)
 * \param[in]    contrast    vary contrast: 1 = lightest; 10 = darkest;
 *                           suggest 1 unless light features are being lost
 * \param[in]    rotation    cw by 90 degrees: {0,1,2,3} represent
 *                           0, 90, 180 and 270 degree cw rotations
 * \param[in]    scale       1 (no scaling) or 2 (2x upscaling)
 * \param[in]    opensize    opening size of structuring element for noise
 *                           removal: {0 or 1 to skip; 2, 3 for opening}
 * \return  cleaned pix, or NULL on error
 *
 * <pre>
 * Notes:
 *    (1) This deskews, optionally rotates and darkens, cleans background
 *        to white, binarizes and optionally removes small noise.
 *    (2) For color and grayscale input, local background normalization is
 *        done to 200, and a threshold of 180 sets the maximum foreground
 *        value in the normalized image.
 *    (3) The %contrast parameter adjusts the binarization to avoid losing
 *        lighter input pixels.  Contrast is increased as %contrast increases
 *        from 1 to 10.
 *    (4) The %scale parameter controls the thresholding to 1 bpp. Two values:
 *            1 = threshold
 *            2 = linear interpolated 2x upscaling before threshold.
 *    (5) The #opensize parameter is the size of a square SEL used with
 *        opening to remove small speckle noise.  Allowed open sizes are 2,3.
 *        If this is to be used, try 2 before 3.
 *    (6) This does the image processing for cleanTo1bppFilesToPdf() and
 *        prog/cleanpdf.c.
 * </pre>
 */
PIX *
pixCleanImage(PIX         *pixs,
              l_int32      contrast,
              l_int32      rotation,
              l_int32      scale,
              l_int32      opensize)
{
char  sequence[32];
PIX  *pix1, *pix2, *pix3, *pix4, *pix5;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (rotation < 0 || rotation > 3) {
        L_ERROR("invalid rotation = %d; rotation must be in  {0,1,2,3}\n",
                __func__, rotation);
        return NULL;
    }
    if (contrast < 1 || contrast > 10) {
        L_ERROR("invalid contrast = %d; contrast must be in [1...10]\n",
                __func__, contrast);
        return NULL;
    }
    if (scale != 1 && scale != 2) {
        L_ERROR("invalid scale = %d; scale must be 1 or 2\n",
                __func__, opensize);
        return NULL;
    }
    if (opensize > 3) {
        L_ERROR("invalid opensize = %d; opensize must be <= 3\n",
                __func__, opensize);
        return NULL;
    }

    if (pixGetDepth(pixs) == 1) {
        if (rotation > 0)
            pix1 = pixRotateOrth(pixs, rotation);
        else
            pix1 = pixClone(pixs);
        pix2 = pixFindSkewAndDeskew(pix1, 2, NULL, NULL);
        if (scale == 2)
            pix4 = pixExpandBinaryReplicate(pix2, 2, 2);
        else  /* scale == 1 */
            pix4 = pixClone(pix2);
    } else {
        pix1 = pixConvertTo8MinMax(pixs);
        if (rotation > 0)
            pix2 = pixRotateOrth(pix1, rotation);
        else
            pix2 = pixClone(pix1);
        pix3 = pixFindSkewAndDeskew(pix2, 2, NULL, NULL);
        pix4 = pixBackgroundNormTo1MinMax(pix3, contrast, scale);
        pixDestroy(&pix3);
    }

    if (opensize == 2 || opensize == 3) {
        snprintf(sequence, sizeof(sequence), "o%d.%d", opensize, opensize);
        pix5 = pixMorphSequence(pix4, sequence, 0);
    } else {
        pix5 = pixClone(pix4);
    }

    pixDestroy(&pix1);
    pixDestroy(&pix2);
    pixDestroy(&pix4);
    return pix5;
}


/*!
 * \brief   pixFindPageForeground()
 *
 * \param[in]    pixs       full resolution (any type or depth)
 * \param[in]    threshold  for binarization; typically about 128
 * \param[in]    mindist    min distance of text from border to allow
 *                          cleaning near border; at 2x reduction, this
 *                          should be larger than 50; typically about 70
 * \param[in]    erasedist  when conditions are satisfied, erase anything
 *                          within this distance of the edge;
 *                          typically 20-30 at 2x reduction
 * \param[in]    showmorph  debug: set to a negative integer to show steps
 *                          in generating masks; this is typically used
 *                          for debugging region extraction
 * \param[in]    pixac      debug: allocate outside and pass this in to
 *                          accumulate results of each call to this function,
 *                          which can be displayed in a mosaic or a pdf.
 * \return  box region including foreground, with some pixel noise
 *                   removed, or NULL if not found
 *
 * <pre>
 * Notes:
 *      (1) This doesn't simply crop to the fg.  It attempts to remove
 *          pixel noise and junk at the edge of the image before cropping.
 *          The input %threshold is used if pixs is not 1 bpp.
 *      (2) This is not intended to work on small thumbnails.  The
 *          dimensions of pixs must be at least MinWidth x MinHeight.
 *      (3) Debug: set showmorph to display the intermediate image in
 *          the morphological operations on this page.
 *      (4) Debug: to get pdf output of results when called repeatedly,
 *          call with an existing pixac, which will add an image of this page,
 *          with the fg outlined.  If no foreground is found, there is
 *          no output for this page image.
 * </pre>
 */
BOX *
pixFindPageForeground(PIX     *pixs,
                      l_int32  threshold,
                      l_int32  mindist,
                      l_int32  erasedist,
                      l_int32  showmorph,
                      PIXAC   *pixac)
{
l_int32  flag, nbox, intersects;
l_int32  w, h, bx, by, bw, bh, left, right, top, bottom;
PIX     *pixb, *pixb2, *pixseed, *pixsf, *pixm, *pix1, *pixg2;
BOX     *box, *boxfg, *boxin, *boxd;
BOXA    *ba1, *ba2;

    if (!pixs)
        return (BOX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, NULL);
    if (w < MinWidth || h < MinHeight) {
        L_ERROR("pix too small: w = %d, h = %d\n", __func__, w, h);
        return NULL;
    }

        /* Binarize, downscale by 0.5, remove the noise to generate a seed,
         * and do a seedfill back from the seed into those 8-connected
         * components of the binarized image for which there was at least
         * one seed pixel. */
    flag = (showmorph) ? 100 : 0;
    pixb = pixConvertTo1(pixs, threshold);
    pixb2 = pixScale(pixb, 0.5, 0.5);
    pixseed = pixMorphSequence(pixb2, "o1.2 + c9.9 + o3.3", flag);
    pix1 = pixMorphSequence(pixb2, "o50.1", 0);
    pixOr(pixseed, pixseed, pix1);
    pixDestroy(&pix1);
    pix1 = pixMorphSequence(pixb2, "o1.50", 0);
    pixOr(pixseed, pixseed, pix1);
    pixDestroy(&pix1);
    pixsf = pixSeedfillBinary(NULL, pixseed, pixb2, 8);
    pixm = pixRemoveBorderConnComps(pixsf, 8);

        /* Now, where is the main block of text?  We want to remove noise near
         * the edge of the image, but to do that, we have to be convinced that
         * (1) there is noise and (2) it is far enough from the text block
         * and close enough to the edge.  For each edge, if the block
         * is more than mindist from that edge, then clean 'erasedist'
         * pixels from the edge. */
    pix1 = pixMorphSequence(pixm, "c50.50", flag);
    ba1 = pixConnComp(pix1, NULL, 8);
    ba2 = boxaSort(ba1, L_SORT_BY_AREA, L_SORT_DECREASING, NULL);
    pixGetDimensions(pix1, &w, &h, NULL);
    nbox = boxaGetCount(ba2);
    if (nbox > 1) {
        box = boxaGetBox(ba2, 0, L_CLONE);
        boxGetGeometry(box, &bx, &by, &bw, &bh);
        left = (bx > mindist) ? erasedist : 0;
        right = (w - bx - bw > mindist) ? erasedist : 0;
        top = (by > mindist) ? erasedist : 0;
        bottom = (h - by - bh > mindist) ? erasedist : 0;
        pixSetOrClearBorder(pixm, left, right, top, bottom, PIX_CLR);
        boxDestroy(&box);
    }
    pixDestroy(&pix1);
    boxaDestroy(&ba1);
    boxaDestroy(&ba2);

        /* Locate the foreground region; don't bother cropping */
    pixClipToForeground(pixm, NULL, &boxfg);

        /* Sanity check the fg region.  Make sure it's not confined
         * to a thin boundary on the left and right sides of the image,
         * in which case it is likely to be noise. */
    if (boxfg) {
        boxin = boxCreate(0.1 * w, 0, 0.8 * w, h);
        boxIntersects(boxfg, boxin, &intersects);
        boxDestroy(&boxin);
        if (!intersects) boxDestroy(&boxfg);
    }

    boxd = NULL;
    if (boxfg) {
        boxAdjustSides(boxfg, boxfg, -2, 2, -2, 2);  /* tiny expansion */
        boxd = boxTransform(boxfg, 0, 0, 2.0, 2.0);

            /* Save the debug image showing the box for this page */
        if (pixac) {
            pixg2 = pixConvert1To4Cmap(pixb);
            pixRenderBoxArb(pixg2, boxd, 3, 255, 0, 0);
            pixacompAddPix(pixac, pixg2, IFF_DEFAULT);
            pixDestroy(&pixg2);
        }
    }

    pixDestroy(&pixb);
    pixDestroy(&pixb2);
    pixDestroy(&pixseed);
    pixDestroy(&pixsf);
    pixDestroy(&pixm);
    boxDestroy(&boxfg);
    return boxd;
}


/*------------------------------------------------------------------*
 *         Extraction of characters from image with only text       *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixSplitIntoCharacters()
 *
 * \param[in]    pixs      1 bpp, contains only deskewed text
 * \param[in]    minw      min component width for initial filtering; typ. 4
 * \param[in]    minh      min component height for initial filtering; typ. 4
 * \param[out]   pboxa     [optional] character bounding boxes
 * \param[out]   ppixa     [optional] character images
 * \param[out]   ppixdebug [optional] showing splittings
 *
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is a simple function that attempts to find split points
 *          based on vertical pixel profiles.
 *      (2) It should be given an image that has an arbitrary number
 *          of text characters.
 *      (3) The returned pixa includes the boxes from which the
 *          (possibly split) components are extracted.
 * </pre>
 */
l_ok
pixSplitIntoCharacters(PIX     *pixs,
                       l_int32  minw,
                       l_int32  minh,
                       BOXA   **pboxa,
                       PIXA   **ppixa,
                       PIX    **ppixdebug)
{
l_int32  ncomp, i, xoff, yoff;
BOXA   *boxa1, *boxa2, *boxat1, *boxat2, *boxad;
BOXAA  *baa;
PIX    *pix, *pix1, *pix2, *pixdb;
PIXA   *pixa1, *pixadb;

    if (pboxa) *pboxa = NULL;
    if (ppixa) *ppixa = NULL;
    if (ppixdebug) *ppixdebug = NULL;
    if (!pixs || pixGetDepth(pixs) != 1)
        return ERROR_INT("pixs not defined or not 1 bpp", __func__, 1);

        /* Remove the small stuff */
    pix1 = pixSelectBySize(pixs, minw, minh, 8, L_SELECT_IF_BOTH,
                           L_SELECT_IF_GT, NULL);

        /* Small vertical close for consolidation */
    pix2 = pixMorphSequence(pix1, "c1.10", 0);
    pixDestroy(&pix1);

        /* Get the 8-connected components */
    boxa1 = pixConnComp(pix2, &pixa1, 8);
    pixDestroy(&pix2);
    boxaDestroy(&boxa1);

        /* Split the components if obvious */
    ncomp = pixaGetCount(pixa1);
    boxa2 = boxaCreate(ncomp);
    pixadb = (ppixdebug) ? pixaCreate(ncomp) : NULL;
    for (i = 0; i < ncomp; i++) {
        pix = pixaGetPix(pixa1, i, L_CLONE);
        if (ppixdebug) {
            boxat1 = pixSplitComponentWithProfile(pix, 10, 7, &pixdb);
            if (pixdb)
                pixaAddPix(pixadb, pixdb, L_INSERT);
        } else {
            boxat1 = pixSplitComponentWithProfile(pix, 10, 7, NULL);
        }
        pixaGetBoxGeometry(pixa1, i, &xoff, &yoff, NULL, NULL);
        boxat2 = boxaTransform(boxat1, xoff, yoff, 1.0, 1.0);
        boxaJoin(boxa2, boxat2, 0, -1);
        pixDestroy(&pix);
        boxaDestroy(&boxat1);
        boxaDestroy(&boxat2);
    }
    pixaDestroy(&pixa1);

        /* Generate the debug image */
    if (ppixdebug) {
        if (pixaGetCount(pixadb) > 0) {
            *ppixdebug = pixaDisplayTiledInRows(pixadb, 32, 1500,
                                                1.0, 0, 20, 1);
        }
        pixaDestroy(&pixadb);
    }

        /* Do a 2D sort on the bounding boxes, and flatten the result to 1D */
    baa = boxaSort2d(boxa2, NULL, 0, 0, 5);
    boxad = boxaaFlattenToBoxa(baa, NULL, L_CLONE);
    boxaaDestroy(&baa);
    boxaDestroy(&boxa2);

        /* Optionally extract the pieces from the input image */
    if (ppixa)
        *ppixa = pixClipRectangles(pixs, boxad);
    if (pboxa)
        *pboxa = boxad;
    else
        boxaDestroy(&boxad);
    return 0;
}


/*!
 * \brief   pixSplitComponentWithProfile()
 *
 * \param[in]    pixs       1 bpp, exactly one connected component
 * \param[in]    delta      distance used in extrema finding in a numa; typ. 10
 * \param[in]    mindel     minimum required difference between profile
 *                          minimum and profile values +2 and -2 away; typ. 7
 * \param[out]   ppixdebug  [optional] debug image of splitting
 * \return  boxa of c.c. after splitting, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This will split the most obvious cases of touching characters.
 *          The split points it is searching for are narrow and deep
 *          minimima in the vertical pixel projection profile, after a
 *          large vertical closing has been applied to the component.
 * </pre>
 */
BOXA *
pixSplitComponentWithProfile(PIX     *pixs,
                             l_int32  delta,
                             l_int32  mindel,
                             PIX    **ppixdebug)
{
l_int32   w, h, n2, i, firstmin, xmin, xshift;
l_int32   nmin, nleft, nright, nsplit, isplit, ncomp;
l_int32  *array1, *array2;
BOX      *box;
BOXA     *boxad;
NUMA     *na1, *na2, *nasplit;
PIX      *pix1, *pixdb;

    if (ppixdebug) *ppixdebug = NULL;
    if (!pixs || pixGetDepth(pixs) != 1)
        return (BOXA *)ERROR_PTR("pixa undefined or not 1 bpp", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, NULL);

        /* Closing to consolidate characters vertically */
    pix1 = pixCloseSafeBrick(NULL, pixs, 1, 100);

        /* Get extrema of column projections */
    boxad = boxaCreate(2);
    na1 = pixCountPixelsByColumn(pix1);  /* w elements */
    pixDestroy(&pix1);
    na2 = numaFindExtrema(na1, delta, NULL);
    n2 = numaGetCount(na2);
    if (n2 < 3) {  /* no split possible */
        box = boxCreate(0, 0, w, h);
        boxaAddBox(boxad, box, L_INSERT);
        numaDestroy(&na1);
        numaDestroy(&na2);
        return boxad;
    }

        /* Look for sufficiently deep and narrow minima.
         * All minima of of interest must be surrounded by max on each
         * side.  firstmin is the index of first possible minimum. */
    array1 = numaGetIArray(na1);
    array2 = numaGetIArray(na2);
    if (ppixdebug) numaWriteStderr(na2);
    firstmin = (array1[array2[0]] > array1[array2[1]]) ? 1 : 2;
    nasplit = numaCreate(n2);  /* will hold split locations */
    for (i = firstmin; i < n2 - 1; i+= 2) {
        xmin = array2[i];
        nmin = array1[xmin];
        if (xmin + 2 >= w) break;  /* no more splits possible */
        nleft = array1[xmin - 2];
        nright = array1[xmin + 2];
        if (ppixdebug) {
            lept_stderr(
                "Splitting: xmin = %d, w = %d; nl = %d, nmin = %d, nr = %d\n",
                xmin, w, nleft, nmin, nright);
        }
        if (nleft - nmin >= mindel && nright - nmin >= mindel)  /* split */
            numaAddNumber(nasplit, xmin);
    }
    nsplit = numaGetCount(nasplit);

#if 0
    if (ppixdebug && nsplit > 0) {
        lept_mkdir("lept/split");
        gplotSimple1(na1, GPLOT_PNG, "/tmp/lept/split/split", NULL);
    }
#endif

    numaDestroy(&na1);
    numaDestroy(&na2);
    LEPT_FREE(array1);
    LEPT_FREE(array2);

    if (nsplit == 0) {  /* no splitting */
        numaDestroy(&nasplit);
        box = boxCreate(0, 0, w, h);
        boxaAddBox(boxad, box, L_INSERT);
        return boxad;
    }

        /* Use split points to generate b.b. after splitting */
    for (i = 0, xshift = 0; i < nsplit; i++) {
        numaGetIValue(nasplit, i, &isplit);
        box = boxCreate(xshift, 0, isplit - xshift, h);
        boxaAddBox(boxad, box, L_INSERT);
        xshift = isplit + 1;
    }
    box = boxCreate(xshift, 0, w - xshift, h);
    boxaAddBox(boxad, box, L_INSERT);
    numaDestroy(&nasplit);

    if (ppixdebug) {
        pixdb = pixConvertTo32(pixs);
        ncomp = boxaGetCount(boxad);
        for (i = 0; i < ncomp; i++) {
            box = boxaGetBox(boxad, i, L_CLONE);
            pixRenderBoxBlend(pixdb, box, 1, 255, 0, 0, 0.5);
            boxDestroy(&box);
        }
        *ppixdebug = pixdb;
    }

    return boxad;
}


/*------------------------------------------------------------------*
 *                    Extraction of lines of text                   *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixExtractTextlines()
 *
 * \param[in]    pixs        any depth, assumed to have nearly horizontal text
 * \param[in]    maxw, maxh  initial filtering: remove any components in pixs
 *                           with components larger than maxw or maxh
 * \param[in]    minw, minh  final filtering: remove extracted 'lines'
 *                           with sizes smaller than minw or minh; use
 *                           0 for default.
 * \param[in]    adjw, adjh  final adjustment of boxes representing each
 *                           text line.  If > 0, these increase the box
 *                           size at each edge by this amount.
 * \param[in]    pixadb      pixa for saving intermediate steps; NULL to omit
 * \return  pixa of textline images, including bounding boxes, or
 *                    NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This function assumes that textline fragments have sufficient
 *          vertical separation and small enough skew so that a
 *          horizontal dilation sufficient to join words will not join
 *          textlines.  It does not guarantee that horizontally adjacent
 *          textline fragments on the same line will be joined.
 *      (2) For images with multiple columns, it attempts to avoid joining
 *          textlines across the space between columns.  If that is not
 *          a concern, you can also use pixExtractRawTextlines(),
 *          which will join them with alacrity.
 *      (3) This first removes components from pixs that are either
 *          wide (> %maxw) or tall (> %maxh).
 *      (4) A final filtering operation removes small components, such
 *          that width < %minw or height < %minh.
 *      (5) For reasonable accuracy, the resolution of pixs should be
 *          at least 100 ppi.  For reasonable efficiency, the resolution
 *          should not exceed 600 ppi.
 *      (6) This can be used to determine if some region of a scanned
 *          image is horizontal text.
 *      (7) As an example, for a pix with resolution 300 ppi, a reasonable
 *          set of parameters is:
 *             pixExtractTextlines(pix, 150, 150, 36, 20, 5, 5, NULL);
 *          The defaults minw and minh for 300 ppi are about 36 and 20,
 *          so the same result is obtained with:
 *             pixExtractTextlines(pix, 150, 150, 0, 0, 5, 5, NULL);
 *      (8) The output pixa is composed of subimages, one for each textline,
 *          and the boxa in the pixa tells where in %pixs each textline goes.
 * </pre>
 */
PIXA *
pixExtractTextlines(PIX     *pixs,
                    l_int32  maxw,
                    l_int32  maxh,
                    l_int32  minw,
                    l_int32  minh,
                    l_int32  adjw,
                    l_int32  adjh,
                    PIXA    *pixadb)
{
char     buf[64];
l_int32  res, csize, empty;
BOXA    *boxa1, *boxa2, *boxa3;
PIX     *pix1, *pix2, *pix3;
PIXA    *pixa1, *pixa2, *pixa3;

    if (!pixs)
        return (PIXA *)ERROR_PTR("pixs not defined", __func__, NULL);

        /* Binarize carefully, if necessary */
    if (pixGetDepth(pixs) > 1) {
        pix2 = pixConvertTo8(pixs, FALSE);
        pix3 = pixCleanBackgroundToWhite(pix2, NULL, NULL, 1.0, 70, 190);
        pix1 = pixThresholdToBinary(pix3, 150);
        pixDestroy(&pix2);
        pixDestroy(&pix3);
    } else {
        pix1 = pixClone(pixs);
    }
    pixZero(pix1, &empty);
    if (empty) {
        pixDestroy(&pix1);
        L_INFO("no fg pixels in input image\n", __func__);
        return NULL;
    }
    if (pixadb) pixaAddPix(pixadb, pix1, L_COPY);

        /* Remove any very tall or very wide connected components */
    pix2 = pixSelectBySize(pix1, maxw, maxh, 8, L_SELECT_IF_BOTH,
                           L_SELECT_IF_LT, NULL);
    if (pixadb) pixaAddPix(pixadb, pix2, L_COPY);
    pixDestroy(&pix1);

        /* Filter to solidify the text lines within the x-height region.
         * The closing (csize) bridges gaps between words.  The opening
         * removes isolated bridges between textlines. */
    if ((res = pixGetXRes(pixs)) == 0) {
        L_INFO("Resolution is not set: setting to 300 ppi\n", __func__);
        res = 300;
    }
    csize = L_MIN(120., 60.0 * res / 300.0);
    snprintf(buf, sizeof(buf), "c%d.1 + o%d.1", csize, csize / 3);
    pix3 = pixMorphCompSequence(pix2, buf, 0);
    if (pixadb) pixaAddPix(pixadb, pix3, L_COPY);

        /* Extract the connected components.  These should be dilated lines */
    boxa1 = pixConnComp(pix3, &pixa1, 4);
    if (pixadb) {
        pix1 = pixaDisplayRandomCmap(pixa1, 0, 0);
        pixcmapResetColor(pixGetColormap(pix1), 0, 255, 255, 255);
        pixaAddPix(pixadb, pix1, L_INSERT);
    }

        /* Set minw, minh if default is requested */
    minw = (minw != 0) ? minw : (l_int32)(0.12 * res);
    minh = (minh != 0) ? minh : (l_int32)(0.07 * res);

        /* Remove line components that are too small */
    pixa2 = pixaSelectBySize(pixa1, minw, minh, L_SELECT_IF_BOTH,
                           L_SELECT_IF_GTE, NULL);
    if (pixadb) {
        pix1 = pixaDisplayRandomCmap(pixa2, 0, 0);
        pixcmapResetColor(pixGetColormap(pix1), 0, 255, 255, 255);
        pixaAddPix(pixadb, pix1, L_INSERT);
        pix1 = pixConvertTo32(pix2);
        pixRenderBoxaArb(pix1, pixa2->boxa, 2, 255, 0, 0);
        pixaAddPix(pixadb, pix1, L_INSERT);
    }

        /* Selectively AND with the version before dilation, and save */
    boxa2 = pixaGetBoxa(pixa2, L_CLONE);
    boxa3 = boxaAdjustSides(boxa2, -adjw, adjw, -adjh, adjh);
    pixa3 = pixClipRectangles(pix2, boxa3);
    if (pixadb) {
        pix1 = pixaDisplayRandomCmap(pixa3, 0, 0);
        pixcmapResetColor(pixGetColormap(pix1), 0, 255, 255, 255);
        pixaAddPix(pixadb, pix1, L_INSERT);
    }

    pixDestroy(&pix2);
    pixDestroy(&pix3);
    pixaDestroy(&pixa1);
    pixaDestroy(&pixa2);
    boxaDestroy(&boxa1);
    boxaDestroy(&boxa2);
    boxaDestroy(&boxa3);
    return pixa3;
}


/*!
 * \brief   pixExtractRawTextlines()
 *
 * \param[in]    pixs        any depth, assumed to have nearly horizontal text
 * \param[in]    maxw, maxh  initial filtering: remove any components in pixs
 *                           with components larger than maxw or maxh;
 *                           use 0 for default values.
 * \param[in]    adjw, adjh  final adjustment of boxes representing each
 *                           text line.  If > 0, these increase the box
 *                           size at each edge by this amount.
 * \param[in]    pixadb      pixa for saving intermediate steps; NULL to omit
 * \return  pixa of textline images, including bounding boxes, or
 *                    NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This function assumes that textlines have sufficient
 *          vertical separation and small enough skew so that a
 *          horizontal dilation sufficient to join words will not join
 *          textlines.  It aggressively joins textlines across multiple
 *          columns, so if that is not desired, you must either (a) make
 *          sure that %pixs is a single column of text or (b) use instead
 *          pixExtractTextlines(), which is more conservative
 *          about joining text fragments that have vertical overlap.
 *      (2) This first removes components from pixs that are either
 *          very wide (> %maxw) or very tall (> %maxh).
 *      (3) For reasonable accuracy, the resolution of pixs should be
 *          at least 100 ppi.  For reasonable efficiency, the resolution
 *          should not exceed 600 ppi.
 *      (4) This can be used to determine if some region of a scanned
 *          image is horizontal text.
 *      (5) As an example, for a pix with resolution 300 ppi, a reasonable
 *          set of parameters is:
 *             pixExtractRawTextlines(pix, 150, 150, 0, 0, NULL);
 *      (6) The output pixa is composed of subimages, one for each textline,
 *          and the boxa in the pixa tells where in %pixs each textline goes.
 * </pre>
 */
PIXA *
pixExtractRawTextlines(PIX     *pixs,
                       l_int32  maxw,
                       l_int32  maxh,
                       l_int32  adjw,
                       l_int32  adjh,
                       PIXA    *pixadb)
{
char     buf[64];
l_int32  res, csize, empty;
BOXA    *boxa1, *boxa2, *boxa3;
BOXAA   *baa1;
PIX     *pix1, *pix2, *pix3;
PIXA    *pixa1, *pixa2;

    if (!pixs)
        return (PIXA *)ERROR_PTR("pixs not defined", __func__, NULL);

        /* Set maxw, maxh if default is requested */
    if ((res = pixGetXRes(pixs)) == 0) {
        L_INFO("Resolution is not set: setting to 300 ppi\n", __func__);
        res = 300;
    }
    maxw = (maxw != 0) ? maxw : (l_int32)(0.5 * res);
    maxh = (maxh != 0) ? maxh : (l_int32)(0.5 * res);

        /* Binarize carefully, if necessary */
    if (pixGetDepth(pixs) > 1) {
        pix2 = pixConvertTo8(pixs, FALSE);
        pix3 = pixCleanBackgroundToWhite(pix2, NULL, NULL, 1.0, 70, 190);
        pix1 = pixThresholdToBinary(pix3, 150);
        pixDestroy(&pix2);
        pixDestroy(&pix3);
    } else {
        pix1 = pixClone(pixs);
    }
    pixZero(pix1, &empty);
    if (empty) {
        pixDestroy(&pix1);
        L_INFO("no fg pixels in input image\n", __func__);
        return NULL;
    }
    if (pixadb) pixaAddPix(pixadb, pix1, L_COPY);

        /* Remove any very tall or very wide connected components */
    pix2 = pixSelectBySize(pix1, maxw, maxh, 8, L_SELECT_IF_BOTH,
                           L_SELECT_IF_LT, NULL);
    if (pixadb) pixaAddPix(pixadb, pix2, L_COPY);
    pixDestroy(&pix1);

        /* Filter to solidify the text lines within the x-height region.
         * The closing (csize) bridges gaps between words. */
    csize = L_MIN(120., 60.0 * res / 300.0);
    snprintf(buf, sizeof(buf), "c%d.1", csize);
    pix3 = pixMorphCompSequence(pix2, buf, 0);
    if (pixadb) pixaAddPix(pixadb, pix3, L_COPY);

        /* Extract the connected components.  These should be dilated lines */
    boxa1 = pixConnComp(pix3, &pixa1, 4);
    if (pixadb) {
        pix1 = pixaDisplayRandomCmap(pixa1, 0, 0);
        pixcmapResetColor(pixGetColormap(pix1), 0, 255, 255, 255);
        pixaAddPix(pixadb, pix1, L_INSERT);
    }

        /* Do a 2-d sort, and generate a bounding box for each set of text
         * line segments that is aligned horizontally (i.e., has vertical
         * overlap) into a box representing a single text line. */
    baa1 = boxaSort2d(boxa1, NULL, -1, -1, 5);
    boxaaGetExtent(baa1, NULL, NULL, NULL, &boxa2);
    if (pixadb) {
        pix1 = pixConvertTo32(pix2);
        pixRenderBoxaArb(pix1, boxa2, 2, 255, 0, 0);
        pixaAddPix(pixadb, pix1, L_INSERT);
    }

        /* Optionally adjust the sides of each text line box, and then
         * use the boxes to generate a pixa of the text lines. */
    boxa3 = boxaAdjustSides(boxa2, -adjw, adjw, -adjh, adjh);
    pixa2 = pixClipRectangles(pix2, boxa3);
    if (pixadb) {
        pix1 = pixaDisplayRandomCmap(pixa2, 0, 0);
        pixcmapResetColor(pixGetColormap(pix1), 0, 255, 255, 255);
        pixaAddPix(pixadb, pix1, L_INSERT);
    }

    pixDestroy(&pix2);
    pixDestroy(&pix3);
    pixaDestroy(&pixa1);
    boxaDestroy(&boxa1);
    boxaDestroy(&boxa2);
    boxaDestroy(&boxa3);
    boxaaDestroy(&baa1);
    return pixa2;
}


/*------------------------------------------------------------------*
 *                      How many text columns                       *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixCountTextColumns()
 *
 * \param[in]    pixs        1 bpp
 * \param[in]    deltafract  fraction of (max - min) to be used in the delta
 *                           for extrema finding; typ 0.3
 * \param[in]    peakfract   fraction of (max - min) to be used to threshold
 *                            the peak value; typ. 0.5
 * \param[in]    clipfract   fraction of image dimension removed on each side;
 *                           typ. 0.1, which leaves w and h reduced by 0.8
 * \param[out]   pncols      number of columns; -1 if not determined
 * \param[in]    pixadb      [optional] pre-allocated, for showing
 *                           intermediate computation; use null to skip
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) It is assumed that pixs has the correct resolution set.
 *          If the resolution is 0, we set to 300 and issue a warning.
 *      (2) If necessary, the image is scaled to between 37 and 75 ppi;
 *          most of the processing is done at this resolution.
 *      (3) If no text is found (essentially a blank page),
 *          this returns ncols = 0.
 *      (4) For debug output, input a pre-allocated pixa.
 * </pre>
 */
l_ok
pixCountTextColumns(PIX       *pixs,
                    l_float32  deltafract,
                    l_float32  peakfract,
                    l_float32  clipfract,
                    l_int32   *pncols,
                    PIXA      *pixadb)
{
l_int32    w, h, res, i, n, npeak;
l_float32  scalefact, redfact, minval, maxval, val4, val5, fract;
BOX       *box;
NUMA      *na1, *na2, *na3, *na4, *na5;
PIX       *pix1, *pix2, *pix3, *pix4, *pix5;

    if (!pncols)
        return ERROR_INT("&ncols not defined", __func__, 1);
    *pncols = -1;  /* init */
    if (!pixs || pixGetDepth(pixs) != 1)
        return ERROR_INT("pixs not defined or not 1 bpp", __func__, 1);
    if (deltafract < 0.15 || deltafract > 0.75)
        L_WARNING("deltafract not in [0.15 ... 0.75]\n", __func__);
    if (peakfract < 0.25 || peakfract > 0.9)
        L_WARNING("peakfract not in [0.25 ... 0.9]\n", __func__);
    if (clipfract < 0.0 || clipfract >= 0.5)
        return ERROR_INT("clipfract not in [0.0 ... 0.5)\n", __func__, 1);
    if (pixadb) pixaAddPix(pixadb, pixs, L_COPY);

        /* Scale to between 37.5 and 75 ppi */
    if ((res = pixGetXRes(pixs)) == 0) {
        L_WARNING("resolution undefined; set to 300\n", __func__);
        pixSetResolution(pixs, 300, 300);
        res = 300;
    }
    if (res < 37) {
        L_WARNING("resolution %d very low\n", __func__, res);
        scalefact = 37.5 / res;
        pix1 = pixScale(pixs, scalefact, scalefact);
    } else {
        redfact = (l_float32)res / 37.5;
        if (redfact < 2.0)
            pix1 = pixClone(pixs);
        else if (redfact < 4.0)
            pix1 = pixReduceRankBinaryCascade(pixs, 1, 0, 0, 0);
        else if (redfact < 8.0)
            pix1 = pixReduceRankBinaryCascade(pixs, 1, 2, 0, 0);
        else if (redfact < 16.0)
            pix1 = pixReduceRankBinaryCascade(pixs, 1, 2, 2, 0);
        else
            pix1 = pixReduceRankBinaryCascade(pixs, 1, 2, 2, 2);
    }
    if (pixadb) pixaAddPix(pixadb, pix1, L_COPY);

        /* Crop inner 80% of image */
    pixGetDimensions(pix1, &w, &h, NULL);
    box = boxCreate(clipfract * w, clipfract * h,
                    (1.0 - 2 * clipfract) * w, (1.0 - 2 * clipfract) * h);
    pix2 = pixClipRectangle(pix1, box, NULL);
    pixGetDimensions(pix2, &w, &h, NULL);
    boxDestroy(&box);
    if (pixadb) pixaAddPix(pixadb, pix2, L_COPY);

        /* Deskew */
    pix3 = pixDeskew(pix2, 0);
    if (pixadb) pixaAddPix(pixadb, pix3, L_COPY);

        /* Close to increase column counts for text */
    pix4 = pixCloseSafeBrick(NULL, pix3, 5, 21);
    if (pixadb) pixaAddPix(pixadb, pix4, L_COPY);
    pixInvert(pix4, pix4);
    na1 = pixCountByColumn(pix4, NULL);

    if (pixadb) {
        gplotSimple1(na1, GPLOT_PNG, "/tmp/lept/plot", NULL);
        pix5 = pixRead("/tmp/lept/plot.png");
        pixaAddPix(pixadb, pix5, L_INSERT);
    }

        /* Analyze the column counts.  na4 gives the locations of
         * the extrema in normalized units (0.0 to 1.0) across the
         * cropped image.  na5 gives the magnitude of the
         * extrema, normalized to the dynamic range.  The peaks
         * are values that are at least peakfract of (max - min). */
    numaGetMax(na1, &maxval, NULL);
    numaGetMin(na1, &minval, NULL);
    fract = (l_float32)(maxval - minval) / h;  /* is there much at all? */
    if (fract < 0.05) {
        L_INFO("very little content on page; 0 text columns\n", __func__);
        *pncols = 0;
    } else {
        na2 = numaFindExtrema(na1, deltafract * (maxval - minval), &na3);
        na4 = numaTransform(na2, 0, 1.0 / w);
        na5 = numaTransform(na3, -minval, 1.0 / (maxval - minval));
        n = numaGetCount(na4);
        for (i = 0, npeak = 0; i < n; i++) {
            numaGetFValue(na4, i, &val4);
            numaGetFValue(na5, i, &val5);
            if (val4 > 0.3 && val4 < 0.7 && val5 >= peakfract) {
                npeak++;
                L_INFO("Peak(loc,val) = (%5.3f,%5.3f)\n", __func__, val4, val5);
            }
        }
        *pncols = npeak + 1;
        numaDestroy(&na2);
        numaDestroy(&na3);
        numaDestroy(&na4);
        numaDestroy(&na5);
    }

    pixDestroy(&pix1);
    pixDestroy(&pix2);
    pixDestroy(&pix3);
    pixDestroy(&pix4);
    numaDestroy(&na1);
    return 0;
}


/*------------------------------------------------------------------*
 *                      Decision text vs photo                      *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixDecideIfText()
 *
 * \param[in]    pixs     any depth
 * \param[in]    box      [optional]  if null, use entire pixs
 * \param[out]   pistext  1 if text; 0 if photo; -1 if not determined or empty
 * \param[in]    pixadb   [optional] pre-allocated, for showing intermediate
 *                        computation; use NULL to skip
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) It is assumed that pixs has the correct resolution set.
 *          If the resolution is 0, we set to 300 and issue a warning.
 *      (2) If necessary, the image is scaled to 300 ppi; most of the
 *          processing is done at this resolution.
 *      (3) Text is assumed to be in horizontal lines.
 *      (4) Because thin vertical lines are removed before filtering for
 *          text lines, this should identify tables as text.
 *      (5) If %box is null and pixs contains both text lines and line art,
 *          this function might return %istext == true.
 *      (6) If the input pixs is empty, or for some other reason the
 *          result can not be determined, return -1.
 *      (7) For debug output, input a pre-allocated pixa.
 * </pre>
 */
l_ok
pixDecideIfText(PIX      *pixs,
                BOX      *box,
                l_int32  *pistext,
                PIXA     *pixadb)
{
l_int32    i, empty, maxw, w, h, n1, n2, n3, minlines, big_comp;
l_float32  ratio1, ratio2;
L_BMF     *bmf;
BOXA      *boxa1, *boxa2, *boxa3, *boxa4, *boxa5;
PIX       *pix1, *pix2, *pix3, *pix4, *pix5, *pix6, *pix7;
PIXA      *pixa1;
SEL       *sel1;

    if (!pistext)
        return ERROR_INT("&istext not defined", __func__, 1);
    *pistext = -1;
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);

        /* Crop, convert to 1 bpp, 300 ppi */
    if ((pix1 = pixPrepare1bpp(pixs, box, 0.1, 300)) == NULL)
        return ERROR_INT("pix1 not made", __func__, 1);

    pixZero(pix1, &empty);
    if (empty) {
        pixDestroy(&pix1);
        L_INFO("pix is empty\n", __func__);
        return 0;
    }
    w = pixGetWidth(pix1);

        /* Identify and remove tall, thin vertical lines (as found in tables)
         * that are up to 9 pixels wide.  Make a hit-miss sel with an
         * 81 pixel vertical set of hits and with 3 pairs of misses that
         * are 10 pixels apart horizontally.  It is necessary to use a
         * hit-miss transform; if we only opened with a vertical line of
         * hits, we would remove solid regions of pixels that are not
         * text or vertical lines. */
    pix2 = pixCreate(11, 81, 1);
    for (i = 0; i < 81; i++)
        pixSetPixel(pix2, 5, i, 1);
    sel1 = selCreateFromPix(pix2, 40, 5, NULL);
    selSetElement(sel1, 20, 0, SEL_MISS);
    selSetElement(sel1, 20, 10, SEL_MISS);
    selSetElement(sel1, 40, 0, SEL_MISS);
    selSetElement(sel1, 40, 10, SEL_MISS);
    selSetElement(sel1, 60, 0, SEL_MISS);
    selSetElement(sel1, 60, 10, SEL_MISS);
    pix3 = pixHMT(NULL, pix1, sel1);
    pix4 = pixSeedfillBinaryRestricted(NULL, pix3, pix1, 8, 5, 1000);
    pix5 = pixXor(NULL, pix1, pix4);
    pixDestroy(&pix2);
    selDestroy(&sel1);

        /* Convert the text lines to separate long horizontal components */
    pix6 = pixMorphCompSequence(pix5, "c30.1 + o15.1 + c60.1 + o2.2", 0);

        /* Estimate the distance to the bottom of the significant region */
    if (box) {  /* use full height */
        pixGetDimensions(pix6, NULL, &h, NULL);
    } else {  /* use height of region that has text lines */
        pixFindThreshFgExtent(pix6, 400, NULL, &h);
    }

    if (pixadb) {
        bmf = bmfCreate(NULL, 6);
        pixaAddPixWithText(pixadb, pix1, 1, bmf, "threshold/crop to binary",
                           0x0000ff00, L_ADD_BELOW);
        pixaAddPixWithText(pixadb, pix3, 2, bmf, "hit-miss for vertical line",
                           0x0000ff00, L_ADD_BELOW);
        pixaAddPixWithText(pixadb, pix4, 2, bmf, "restricted seed-fill",
                           0x0000ff00, L_ADD_BELOW);
        pixaAddPixWithText(pixadb, pix5, 2, bmf, "remove using xor",
                           0x0000ff00, L_ADD_BELOW);
        pixaAddPixWithText(pixadb, pix6, 2, bmf, "make long horiz components",
                           0x0000ff00, L_ADD_BELOW);
    }

        /* Extract the connected components */
    if (pixadb) {
        boxa1 = pixConnComp(pix6, &pixa1, 8);
        pix7 = pixaDisplayRandomCmap(pixa1, 0, 0);
        pixcmapResetColor(pixGetColormap(pix7), 0, 255, 255, 255);
        pixaAddPixWithText(pixadb, pix7, 2, bmf, "show connected components",
                           0x0000ff00, L_ADD_BELOW);
        pixDestroy(&pix7);
        pixaDestroy(&pixa1);
        bmfDestroy(&bmf);
    } else {
        boxa1 = pixConnComp(pix6, NULL, 8);
    }

        /* Analyze the connected components.  The following conditions
         * at 300 ppi must be satisfied if the image is text:
         * (1) There are no components that are wider than 400 pixels and
         *     taller than 175 pixels.
         * (2) The second longest component is at least 60% of the
         *     (possibly cropped) image width.  This catches images
         *     that don't have any significant content.
         * (3) Of the components that are at least 40% of the length
         *     of the longest (n2), at least 80% of them must not exceed
         *     60 pixels in height.
         * (4) The number of those long, thin components (n3) must
         *     equal or exceed a minimum that scales linearly with the
         *     image height.
         * Most images that are not text fail more than one of these
         * conditions. */
    boxa2 = boxaSort(boxa1, L_SORT_BY_WIDTH, L_SORT_DECREASING, NULL);
    boxaGetBoxGeometry(boxa2, 1, NULL, NULL, &maxw, NULL);  /* 2nd longest */
    boxa3 = boxaSelectBySize(boxa1, 0.4 * maxw, 0, L_SELECT_WIDTH,
                             L_SELECT_IF_GTE, NULL);
    boxa4 = boxaSelectBySize(boxa3, 0, 60, L_SELECT_HEIGHT,
                             L_SELECT_IF_LTE, NULL);
    boxa5 = boxaSelectBySize(boxa1, 400, 175, L_SELECT_IF_BOTH,
                             L_SELECT_IF_GT, NULL);
    big_comp = (boxaGetCount(boxa5) == 0) ? 0 : 1;
    n1 = boxaGetCount(boxa1);
    n2 = boxaGetCount(boxa3);
    n3 = boxaGetCount(boxa4);
    ratio1 = (l_float32)maxw / (l_float32)w;
    ratio2 = (l_float32)n3 / (l_float32)n2;
    minlines = L_MAX(2, h / 125);
    if (big_comp || ratio1 < 0.6 || ratio2 < 0.8 || n3 < minlines)
        *pistext = 0;
    else
        *pistext = 1;
    if (pixadb) {
        if (*pistext == 1) {
            L_INFO("This is text: \n  n1 = %d, n2 = %d, n3 = %d, "
                   "minlines = %d\n  maxw = %d, ratio1 = %4.2f, h = %d, "
                   "big_comp = %d\n", __func__, n1, n2, n3, minlines,
                   maxw, ratio1, h, big_comp);
        } else {
            L_INFO("This is not text: \n  n1 = %d, n2 = %d, n3 = %d, "
                   "minlines = %d\n  maxw = %d, ratio1 = %4.2f, h = %d, "
                   "big_comp = %d\n", __func__, n1, n2, n3, minlines,
                   maxw, ratio1, h, big_comp);
        }
    }

    boxaDestroy(&boxa1);
    boxaDestroy(&boxa2);
    boxaDestroy(&boxa3);
    boxaDestroy(&boxa4);
    boxaDestroy(&boxa5);
    pixDestroy(&pix1);
    pixDestroy(&pix3);
    pixDestroy(&pix4);
    pixDestroy(&pix5);
    pixDestroy(&pix6);
    return 0;
}


/*!
 * \brief   pixFindThreshFgExtent()
 *
 * \param[in]    pixs     1 bpp
 * \param[in]    thresh   threshold number of pixels in row
 * \param[out]   ptop     [optional] location of top of region
 * \param[out]   pbot     [optional] location of bottom of region
 * \return  0 if OK, 1 on error
 */
l_ok
pixFindThreshFgExtent(PIX      *pixs,
                      l_int32   thresh,
                      l_int32  *ptop,
                      l_int32  *pbot)
{
l_int32   i, n;
l_int32  *array;
NUMA     *na;

    if (ptop) *ptop = 0;
    if (pbot) *pbot = 0;
    if (!ptop && !pbot)
        return ERROR_INT("nothing to determine", __func__, 1);
    if (!pixs || pixGetDepth(pixs) != 1)
        return ERROR_INT("pixs not defined or not 1 bpp", __func__, 1);

    na = pixCountPixelsByRow(pixs, NULL);
    n = numaGetCount(na);
    array = numaGetIArray(na);
    if (ptop) {
        for (i = 0; i < n; i++) {
            if (array[i] >= thresh) {
                *ptop = i;
                break;
            }
        }
    }
    if (pbot) {
        for (i = n - 1; i >= 0; i--) {
            if (array[i] >= thresh) {
                *pbot = i;
                break;
            }
        }
    }
    LEPT_FREE(array);
    numaDestroy(&na);
    return 0;
}


/*------------------------------------------------------------------*
 *                     Decision: table vs text                      *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixDecideIfTable()
 *
 * \param[in]    pixs      any depth, any resolution >= 75 ppi
 * \param[in]    box       [optional] if null, use entire pixs
 * \param[in]    orient    L_PORTRAIT_MODE, L_LANDSCAPE_MODE
 * \param[out]   pscore    0 - 4; -1 if not determined
 * \param[in]    pixadb    [optional] pre-allocated, for showing intermediate
 *                         computation; use NULL to skip
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) It is assumed that pixs has the correct resolution set.
 *          If the resolution is 0, we assume it is 300 ppi and issue a warning.
 *      (2) If %orient == L_LANDSCAPE_MODE, the image is rotated 90 degrees
 *          clockwise before being analyzed.
 *      (3) The interpretation of the returned score:
 *            -1     undetermined
 *             0     no table
 *             1     unlikely to have a table
 *             2     likely to have a table
 *             3     even more likely to have a table
 *             4     extremely likely to have a table
 *          * Setting the condition for finding a table at score >= 2 works
 *            well, except for false positives on kanji and landscape text.
 *          * These false positives can be removed by setting the condition
 *            at score >= 3, but recall is lowered because it will not find
 *            tables without either horizontal or vertical lines.
 *      (4) Most of the processing takes place at 75 ppi.
 *      (5) Internally, three numbers are determined, for horizontal and
 *          vertical fg lines, and for vertical bg lines.  From these,
 *          four tests are made to decide if there is a table occupying
 *          a significant part of the image.
 *      (6) Images have arbitrary content and would be likely to trigger
 *          this detector, so they are checked for first, and if found,
 *          return with a 0 (no table) score.
 *      (7) Musical scores (tablature) are likely to trigger the detector.
 *      (8) Tables of content with more than 2 columns are likely to
 *          trigger the detector.
 *      (9) For debug output, input a pre-allocated pixa.
 * </pre>
 */
l_ok
pixDecideIfTable(PIX      *pixs,
                 BOX      *box,
                 l_int32   orient,
                 l_int32  *pscore,
                 PIXA     *pixadb)
{
l_int32  empty, nhb, nvb, nvw, score, htfound;
PIX     *pix1, *pix2, *pix3, *pix4, *pix5, *pix6, *pix7, *pix8, *pix9;

    if (!pscore)
        return ERROR_INT("&score not defined", __func__, 1);
    *pscore = -1;
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);

        /* Check if there is an image region.  First convert to 1 bpp
         * at 175 ppi.  If an image is found, assume there is no table.  */
    pix1 = pixPrepare1bpp(pixs, box, 0.1f, 175);
    pix2 = pixGenerateHalftoneMask(pix1, NULL, &htfound, NULL);
    if (htfound && pixadb) pixaAddPix(pixadb, pix2, L_COPY);
    pixDestroy(&pix1);
    pixDestroy(&pix2);
    if (htfound) {
        *pscore = 0;
        L_INFO("pix has an image region\n", __func__);
        return 0;
    }

        /* Crop, convert to 1 bpp, 75 ppi */
    if ((pix1 = pixPrepare1bpp(pixs, box, 0.05, 75)) == NULL)
        return ERROR_INT("pix1 not made", __func__, 1);

    pixZero(pix1, &empty);
    if (empty) {
        *pscore = 0;
        pixDestroy(&pix1);
        L_INFO("pix is empty\n", __func__);
        return 0;
    }

        /* The 2x2 dilation on 75 ppi makes these two approaches very similar:
         * (1) pix1 = pixPrepare1bpp(..., 300);  // 300 ppi resolution
         *     pix2 = pixReduceRankBinaryCascade(pix1, 1, 1, 0, 0);
         * (2) pix1 = pixPrepare1bpp(..., 75);  // 75 ppi resolution
         *     pix2 = pixDilateBrick(NULL, pix1, 2, 2);
         * But (2) is more efficient if the input image to pixPrepare1bpp()
         * is not at 300 ppi.   */
    pix2 = pixDilateBrick(NULL, pix1, 2, 2);

        /* Deskew both horizontally and vertically; rotate by 90
         * degrees if in landscape mode. */
    pix3 = pixDeskewBoth(pix2, 1);
    if (pixadb) {
        pixaAddPix(pixadb, pix2, L_COPY);
        pixaAddPix(pixadb, pix3, L_COPY);
    }
    if (orient == L_LANDSCAPE_MODE)
        pix4 = pixRotate90(pix3, 1);
    else
        pix4 = pixClone(pix3);
    pixDestroy(&pix1);
    pixDestroy(&pix2);
    pixDestroy(&pix3);
    pix1 = pixClone(pix4);
    pixDestroy(&pix4);

        /* Look for horizontal and vertical lines */
    pix2 = pixMorphSequence(pix1, "o100.1 + c1.4", 0);
    pix3 = pixSeedfillBinary(NULL, pix2, pix1, 8);
    pix4 = pixMorphSequence(pix1, "o1.100 + c4.1", 0);
    pix5 = pixSeedfillBinary(NULL, pix4, pix1, 8);
    pix6 = pixOr(NULL, pix3, pix5);
    if (pixadb) {
        pixaAddPix(pixadb, pix2, L_COPY);
        pixaAddPix(pixadb, pix4, L_COPY);
        pixaAddPix(pixadb, pix3, L_COPY);
        pixaAddPix(pixadb, pix5, L_COPY);
        pixaAddPix(pixadb, pix6, L_COPY);
    }
    pixCountConnComp(pix2, 8, &nhb);  /* number of horizontal black lines */
    pixCountConnComp(pix4, 8, &nvb);  /* number of vertical black lines */

        /* Remove the lines */
    pixSubtract(pix1, pix1, pix6);
    if (pixadb) pixaAddPix(pixadb, pix1, L_COPY);

        /* Remove noise pixels */
    pix7 = pixMorphSequence(pix1, "c4.1 + o8.1", 0);
    if (pixadb) pixaAddPix(pixadb, pix7, L_COPY);

        /* Look for vertical white space.  Invert to convert white bg
         * to fg.  Use a single rank-1 2x reduction, which closes small
         * fg holes, for the final processing at 37.5 ppi.
         * The vertical opening is then about 3 inches on a 300 ppi image.
         * We also remove vertical whitespace that is less than 5 pixels
         * wide at this resolution (about 0.1 inches) */
    pixInvert(pix7, pix7);
    pix8 = pixMorphSequence(pix7, "r1 + o1.100", 0);
    pix9 = pixSelectBySize(pix8, 5, 0, 8, L_SELECT_WIDTH,
                           L_SELECT_IF_GTE, NULL);
    pixCountConnComp(pix9, 8, &nvw);  /* number of vertical white lines */
    if (pixadb) {
        pixaAddPix(pixadb, pixScale(pix8, 2.0, 2.0), L_INSERT);
        pixaAddPix(pixadb, pixScale(pix9, 2.0, 2.0), L_INSERT);
    }

        /* Require at least 2 of the following 4 conditions for a table.
         * Some tables do not have black (fg) lines, and for those we
         * require more than 6 long vertical whitespace (bg) lines.  */
    score = 0;
    if (nhb > 1) score++;
    if (nvb > 2) score++;
    if (nvw > 3) score++;
    if (nvw > 6) score++;
    *pscore = score;

    pixDestroy(&pix1);
    pixDestroy(&pix2);
    pixDestroy(&pix3);
    pixDestroy(&pix4);
    pixDestroy(&pix5);
    pixDestroy(&pix6);
    pixDestroy(&pix7);
    pixDestroy(&pix8);
    pixDestroy(&pix9);
    return 0;
}


/*!
 * \brief   pixPrepare1bpp()
 *
 * \param[in]    pixs       any depth
 * \param[in]    box        [optional] if null, use entire pixs
 * \param[in]    cropfract  fraction to be removed from the boundary;
 *                          use 0.0 to retain the entire image
 * \param[in]    outres     desired resolution of output image; if the
 *                          input image resolution is not set, assume
 *                          300 ppi; use 0 to skip scaling.
 * \return  pixd if OK, NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This handles some common pre-processing operations,
 *          where the page segmentation algorithm takes a 1 bpp image.
 * </pre>
 */
PIX *
pixPrepare1bpp(PIX       *pixs,
               BOX       *box,
               l_float32  cropfract,
               l_int32    outres)
{
l_int32    w, h, res;
l_float32  factor;
BOX       *box1;
PIX       *pix1, *pix2, *pix3, *pix4, *pix5;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);

        /* Crop the image.  If no box is given, use %cropfract to remove
         * pixels near the image boundary; this helps avoid false
         * negatives from noise that is often found there. */
    if (box) {
        pix1 = pixClipRectangle(pixs, box, NULL);
    } else {
        pixGetDimensions(pixs, &w, &h, NULL);
        box1 = boxCreate((l_int32)(cropfract * w), (l_int32)(cropfract * h),
                         (l_int32)((1.0 - 2 * cropfract) * w),
                         (l_int32)((1.0 - 2 * cropfract) * h));
        pix1 = pixClipRectangle(pixs, box1, NULL);
        boxDestroy(&box1);
    }

        /* Convert to 1 bpp with adaptive background cleaning */
    if (pixGetDepth(pixs) > 1) {
        pix2 = pixConvertTo8(pix1, 0);
        pix3 = pixCleanBackgroundToWhite(pix2, NULL, NULL, 1.0, 70, 160);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
        if (!pix3) {
            L_INFO("pix cleaning failed\n", __func__);
            return NULL;
        }
        pix4 = pixThresholdToBinary(pix3, 200);
        pixDestroy(&pix3);
    } else {
        pix4 = pixClone(pix1);
        pixDestroy(&pix1);
    }

        /* Scale the image to the requested output resolution;
           do not scale if %outres <= 0 */
    if (outres <= 0)
        return pix4;
    if ((res = pixGetXRes(pixs)) == 0) {
        L_WARNING("Resolution is not set: using 300 ppi\n", __func__);
        res = 300;
    }
    if (res != outres) {
        factor = (l_float32)outres / (l_float32)res;
        pix5 = pixScale(pix4, factor, factor);
    } else {
        pix5 = pixClone(pix4);
    }
    pixDestroy(&pix4);
    return pix5;
}


/*------------------------------------------------------------------*
 *               Estimate the grayscale background value            *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixEstimateBackground()
 *
 * \param[in]    pixs         8 bpp, with or without colormap
 * \param[in]    darkthresh   pixels below this value are never considered
 *                            part of the background; typ. 70; use 0 to skip
 * \param[in]    edgecrop     fraction of half-width on each side, and of
 *                            half-height at top and bottom, that are cropped
 * \param[out]   pbg          estimated background, or 0 on error
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Caller should check that return bg value is > 0.
 * </pre>
 */
l_ok
pixEstimateBackground(PIX       *pixs,
                      l_int32    darkthresh,
                      l_float32  edgecrop,
                      l_int32   *pbg)
{
l_int32    w, h, sampling;
l_float32  fbg;
BOX       *box;
PIX       *pix1, *pix2, *pixm;

    if (!pbg)
        return ERROR_INT("&bg not defined", __func__, 1);
    *pbg = 0;
    if (!pixs || pixGetDepth(pixs) != 8)
        return ERROR_INT("pixs not defined or not 8 bpp", __func__, 1);
    if (darkthresh > 128)
        L_WARNING("darkthresh unusually large\n", __func__);
    if (edgecrop < 0.0 || edgecrop >= 1.0)
        return ERROR_INT("edgecrop not in [0.0 ... 1.0)", __func__, 1);

    pix1 = pixRemoveColormap(pixs, REMOVE_CMAP_TO_GRAYSCALE);
    pixGetDimensions(pix1, &w, &h, NULL);

        /* Optionally crop inner part of image */
    if (edgecrop > 0.0) {
        box = boxCreate(0.5 * edgecrop * w, 0.5 * edgecrop * h,
                        (1.0 - edgecrop) * w, (1.0 - edgecrop) * h);
        pix2 = pixClipRectangle(pix1, box, NULL);
        boxDestroy(&box);
    } else {
        pix2 = pixClone(pix1);
    }

        /* We will use no more than 50K samples */
    sampling = L_MAX(1, (l_int32)sqrt((l_float64)(w * h) / 50000. + 0.5));

        /* Optionally make a mask over all pixels lighter than %darkthresh */
    pixm = NULL;
    if (darkthresh > 0) {
        pixm = pixThresholdToBinary(pix2, darkthresh);
        pixInvert(pixm, pixm);
    }

    pixGetRankValueMasked(pix2, pixm, 0, 0, sampling, 0.5, &fbg, NULL);
    *pbg = (l_int32)(fbg + 0.5);
    pixDestroy(&pix1);
    pixDestroy(&pix2);
    pixDestroy(&pixm);
    return 0;
}


/*---------------------------------------------------------------------*
 *             Largest white or black rectangles in an image           *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixFindLargeRectangles()
 *
 * \param[in]    pixs       1 bpp
 * \param[in]    polarity   0 within background, 1 within foreground
 * \param[in]    nrect      number of rectangles to be found
 * \param[out]   pboxa      largest rectangles, sorted by decreasing area
 * \param[in,out]  ppixdb   optional return output with rectangles drawn on it
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This does a greedy search to find the largest rectangles,
 *          either black or white and without overlaps, in %pix.
 *      (2) See pixFindLargestRectangle(), which is called multiple
 *          times, for details.  On each call, the largest rectangle
 *          found is painted, so that none of its pixels can be
 *          used later, before calling it again.
 *      (3) This function is surprisingly fast.  Although
 *          pixFindLargestRectangle() runs at about 50 MPix/sec, when it
 *          is run multiple times by pixFindLargeRectangles(), it processes
 *          at 150 - 250 MPix/sec, and the time is approximately linear
 *          in %nrect.  For example, for a 1 MPix image, searching for
 *          the largest 50 boxes takes about 0.2 seconds.
 * </pre>
 */
l_ok
pixFindLargeRectangles(PIX          *pixs,
                       l_int32       polarity,
                       l_int32       nrect,
                       BOXA        **pboxa,
                       PIX         **ppixdb)
{
l_int32  i, op, bx, by, bw, bh;
BOX     *box;
BOXA    *boxa;
PIX     *pix;

    if (ppixdb) *ppixdb = NULL;
    if (!pboxa)
        return ERROR_INT("&boxa not defined", __func__, 1);
    *pboxa = NULL;
    if (!pixs || pixGetDepth(pixs) != 1)
        return ERROR_INT("pixs not defined or not 1 bpp", __func__, 1);
    if (polarity != 0 && polarity != 1)
        return ERROR_INT("invalid polarity", __func__, 1);
    if (nrect > 1000) {
        L_WARNING("large num rectangles = %d requested; using 1000\n",
                  __func__, nrect);
        nrect = 1000;
    }

    pix = pixCopy(NULL, pixs);
    boxa = boxaCreate(nrect);
    *pboxa = boxa;

        /* Sequentially find largest rectangle and fill with opposite color */
    for (i = 0; i < nrect; i++) {
        if (pixFindLargestRectangle(pix, polarity, &box, NULL) == 1) {
            boxDestroy(&box);
            L_ERROR("failure in pixFindLargestRectangle\n", __func__);
            break;
        }
        boxaAddBox(boxa, box, L_INSERT);
        op = (polarity == 0) ? PIX_SET : PIX_CLR;
        boxGetGeometry(box, &bx, &by, &bw, &bh);
        pixRasterop(pix, bx, by, bw, bh, op, NULL, 0, 0);
    }

    if (ppixdb)
        *ppixdb = pixDrawBoxaRandom(pixs, boxa, 3);

    pixDestroy(&pix);
    return 0;
}


/*!
 * \brief   pixFindLargestRectangle()
 *
 * \param[in]    pixs       1 bpp
 * \param[in]    polarity   0 within background, 1 within foreground
 * \param[out]   pbox       largest area rectangle
 * \param[in,out]  ppixdb   optional return output with rectangle drawn on it
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is a simple and elegant solution to a problem in
 *          computational geometry that at first appears to be quite
 *          difficult: what is the largest rectangle that can be
 *          placed in the image, covering only pixels of one polarity
 *          (bg or fg)?  The solution is O(n), where n is the number
 *          of pixels in the image, and it requires nothing more than
 *          using a simple recursion relation in a single sweep of the image.
 *      (2) In a sweep from UL to LR with left-to-right being the fast
 *          direction, calculate the largest white rectangle at (x, y),
 *          using previously calculated values at pixels #1 and #2:
 *             #1:    (x, y - 1)
 *             #2:    (x - 1, y)
 *          We also need the most recent "black" pixels that were seen
 *          in the current row and column.
 *          Consider the largest area.  There are only two possibilities:
 *             (a)  Min(w(1), horizdist) * (h(1) + 1)
 *             (b)  Min(h(2), vertdist) * (w(2) + 1)
 *          where
 *             horizdist: the distance from the rightmost "black" pixel seen
 *                        in the current row across to the current pixel
 *             vertdist: the distance from the lowest "black" pixel seen
 *                       in the current column down to the current pixel
 *          and we choose the Max of (a) and (b).
 *      (3) To convince yourself that these recursion relations are correct,
 *          it helps to draw the maximum rectangles at #1 and #2.
 *          Then for #1, you try to extend the rectangle down one line,
 *          so that the height is h(1) + 1.  Do you get the full
 *          width of #1, w(1)?  It depends on where the black pixels are
 *          in the current row.  You know the final width is bounded by w(1)
 *          and w(2) + 1, but the actual value depends on the distribution
 *          of black pixels in the current row that are at a distance
 *          from the current pixel that is between these limits.
 *          We call that value "horizdist", and the area is then given
 *          by the expression (a) above.  Using similar reasoning for #2,
 *          where you attempt to extend the rectangle to the right
 *          by 1 pixel, you arrive at (b).  The largest rectangle is
 *          then found by taking the Max.
 * </pre>
 */
l_ok
pixFindLargestRectangle(PIX         *pixs,
                        l_int32      polarity,
                        BOX        **pbox,
                        PIX        **ppixdb)
{
l_int32    i, j, w, h, d, wpls, val;
l_int32    wp, hp, w1, w2, h1, h2, wmin, hmin, area1, area2;
l_int32    xmax, ymax;  /* LR corner of the largest rectangle */
l_int32    maxarea, wmax, hmax, vertdist, horizdist, prevfg;
l_int32   *lowestfg;
l_uint32  *datas, *lines;
l_uint32 **linew, **lineh;
BOX       *box;
PIX       *pixw, *pixh;  /* keeps the width and height for the largest */
                         /* rectangles whose LR corner is located there. */

    if (ppixdb) *ppixdb = NULL;
    if (!pbox)
        return ERROR_INT("&box not defined", __func__, 1);
    *pbox = NULL;
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 1)
        return ERROR_INT("pixs not 1 bpp", __func__, 1);
    if (polarity != 0 && polarity != 1)
        return ERROR_INT("invalid polarity", __func__, 1);

        /* Initialize lowest "fg" seen so far for each column */
    lowestfg = (l_int32 *)LEPT_CALLOC(w, sizeof(l_int32));
    for (i = 0; i < w; i++)
        lowestfg[i] = -1;

        /* The combination (val ^ polarity) is the color for which we
         * are searching for the maximum rectangle.  For polarity == 0,
         * we search in the bg (white). */
    pixw = pixCreate(w, h, 32);  /* stores width */
    pixh = pixCreate(w, h, 32);  /* stores height */
    linew = (l_uint32 **)pixGetLinePtrs(pixw, NULL);
    lineh = (l_uint32 **)pixGetLinePtrs(pixh, NULL);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    maxarea = xmax = ymax = wmax = hmax = 0;
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        prevfg = -1;
        for (j = 0; j < w; j++) {
            val = GET_DATA_BIT(lines, j);
            if ((val ^ polarity) == 0) {  /* bg (0) if polarity == 0, etc. */
                if (i == 0 && j == 0) {
                    wp = hp = 1;
                } else if (i == 0) {
                    wp = linew[i][j - 1] + 1;
                    hp = 1;
                } else if (j == 0) {
                    wp = 1;
                    hp = lineh[i - 1][j] + 1;
                } else {
                        /* Expand #1 prev rectangle down */
                    w1 = linew[i - 1][j];
                    h1 = lineh[i - 1][j];
                    horizdist = j - prevfg;
                    wmin = L_MIN(w1, horizdist);  /* width of new rectangle */
                    area1 = wmin * (h1 + 1);

                        /* Expand #2 prev rectangle to right */
                    w2 = linew[i][j - 1];
                    h2 = lineh[i][j - 1];
                    vertdist = i - lowestfg[j];
                    hmin = L_MIN(h2, vertdist);  /* height of new rectangle */
                    area2 = hmin * (w2 + 1);

                    if (area1 > area2) {
                         wp = wmin;
                         hp = h1 + 1;
                    } else {
                         wp = w2 + 1;
                         hp = hmin;
                    }
                }
            } else {  /* fg (1) if polarity == 0; bg (0) if polarity == 1 */
                prevfg = j;
                lowestfg[j] = i;
                wp = hp = 0;
            }
            linew[i][j] = wp;
            lineh[i][j] = hp;
            if (wp * hp > maxarea) {
                maxarea = wp * hp;
                xmax = j;
                ymax = i;
                wmax = wp;
                hmax = hp;
            }
        }
    }

        /* Translate from LR corner to Box coords (UL corner, w, h) */
    box = boxCreate(xmax - wmax + 1, ymax - hmax + 1, wmax, hmax);
    *pbox = box;

    if (ppixdb) {
        *ppixdb = pixConvertTo8(pixs, TRUE);
        pixRenderHashBoxArb(*ppixdb, box, 6, 2, L_NEG_SLOPE_LINE, 1, 255, 0, 0);
    }

    LEPT_FREE(linew);
    LEPT_FREE(lineh);
    LEPT_FREE(lowestfg);
    pixDestroy(&pixw);
    pixDestroy(&pixh);
    return 0;
}


/*---------------------------------------------------------------------*
 *            Generate rectangle inside connected component            *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixFindRectangleInCC()
 *
 * \param[in]    pixs     1 bpp, with sufficient closings to make the fg be
 *                        a single c.c. that is a convex hull
 * \param[in]    boxs     [optional] if NULL, %pixs should be a minimum
 *                        container of a single c.c.
 * \param[in]    fract    first and all consecutive lines found must be at
 *                        least this fraction of the fast scan dimension
 * \param[in]    dir      L_SCAN_HORIZONTAL, L_SCAN_VERTICAL; direction of
 *                        fast scan
 * \param[in]    select   L_GEOMETRIC_UNION, L_GEOMETRIC_INTERSECTION,
 *                        L_LARGEST_AREA, L_SMALEST_AREA
 * \param[in]    debug    if 1, generates output pdf showing intermediate
 *                        computation and final result
 * \return  box  of included rectangle, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Computation is similar to pixFindLargestRectangle(), but allows
 *          a different set of results to choose from.
 *      (2) Select the fast scan direction.  Then, scanning in the slow
 *          direction, find the longest run of ON pixels in the fast
 *          scan direction and look for the first run that is longer
 *          than %fract of the dimension.  Continue until a shorter run
 *          is found.  This generates a box of ON pixels fitting into the c.c.
 *      (3) Do this from both slow scan directions and use %select to get
 *          a resulting box from these two.
 *      (4) The extracted rectangle is not necessarily the largest that
 *          can fit in the c.c.  To get that, use pixFindLargestRectangle().
 */
BOX *
pixFindRectangleInCC(PIX       *pixs,
                     BOX       *boxs,
                     l_float32  fract,
                     l_int32    dir,
                     l_int32    select,
                     l_int32    debug)
{
l_int32  x, y, i, w, h, w1, h1, w2, h2, found, res;
l_int32  xfirst, xlast, xstart, yfirst, ylast, length;
BOX     *box1, *box2, *box3, *box4, *box5;
PIX     *pix1, *pix2, *pixdb1, *pixdb2;
PIXA    *pixadb;

    if (!pixs || pixGetDepth(pixs) != 1)
        return (BOX *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    if (fract <= 0.0 || fract > 1.0)
        return (BOX *)ERROR_PTR("invalid fraction", __func__, NULL);
    if (dir != L_SCAN_VERTICAL && dir != L_SCAN_HORIZONTAL)
        return (BOX *)ERROR_PTR("invalid scan direction", __func__, NULL);
    if (select != L_GEOMETRIC_UNION && select != L_GEOMETRIC_INTERSECTION &&
        select != L_LARGEST_AREA && select != L_SMALLEST_AREA)
        return (BOX *)ERROR_PTR("invalid select", __func__, NULL);

        /* Extract the c.c. if necessary */
    x = y = 0;
    if (boxs) {
        pix1 = pixClipRectangle(pixs, boxs, NULL);
        boxGetGeometry(boxs, &x, &y, NULL, NULL);
    } else {
        pix1 = pixClone(pixs);
    }

        /* All fast scans are horizontal; rotate 90 deg cw if necessary */
    if (dir == L_SCAN_VERTICAL)
        pix2 = pixRotate90(pix1, 1);
    else  /* L_SCAN_HORIZONTAL */
        pix2 = pixClone(pix1);
    pixGetDimensions(pix2, &w, &h, NULL);

    pixadb = (debug) ? pixaCreate(0) : NULL;
    pixdb1 = NULL;
    if (pixadb) {
        lept_mkdir("lept/rect");
        pixaAddPix(pixadb, pix1, L_CLONE);
        pixdb1 = pixConvertTo32(pix2);
    }
    pixDestroy(&pix1);

        /* Scanning down, find the first scanline with a long enough run.
         * That run goes from (xfirst, yfirst) to (xlast, yfirst).  */
    found = FALSE;
    for (i = 0; i < h; i++) {
        pixFindMaxHorizontalRunOnLine(pix2, i, &xstart, &length);
        if (length >= (l_int32)(fract * w + 0.5)) {
            yfirst = i;
            xfirst = xstart;
            xlast = xfirst + length - 1;
            found = TRUE;
            break;
        }
    }
    if (!found) {
        L_WARNING("no run of sufficient size was found\n", __func__);
        pixDestroy(&pix2);
        pixDestroy(&pixdb1);
        pixaDestroy(&pixadb);
        return NULL;
    }

         /* Continue down until the condition fails */
    w1 = xlast - xfirst + 1;
    h1 = h - yfirst;  /* init */
    ylast = h - 1;  /* init */
    for (i = yfirst + 1; i < h; i++) {
        pixFindMaxHorizontalRunOnLine(pix2, i, &xstart, &length);
        if (xstart > xfirst || (xstart + length - 1 < xlast) ||
            i == h - 1) {
            ylast = i - 1;
            h1 = ylast - yfirst + 1;
            break;
        }
    }
    box1 = boxCreate(xfirst, yfirst, w1, h1);

        /* Scanning up, find the first scanline with a long enough run.
         * That run goes from (xfirst, ylast) to (xlast, ylast).  */
    for (i = h - 1; i >= 0; i--) {
        pixFindMaxHorizontalRunOnLine(pix2, i, &xstart, &length);
        if (length >= (l_int32)(fract * w + 0.5)) {
            ylast = i;
            xfirst = xstart;
            xlast = xfirst + length - 1;
            break;
        }
    }

         /* Continue up until the condition fails */
    w2 = xlast - xfirst + 1;
    h2 = ylast + 1;  /* initialize */
    for (i = ylast - 1; i >= 0; i--) {
        pixFindMaxHorizontalRunOnLine(pix2, i, &xstart, &length);
        if (xstart > xfirst || (xstart + length - 1 < xlast) ||
            i == 0) {
            yfirst = i + 1;
            h2 = ylast - yfirst + 1;
            break;
        }
    }
    box2 = boxCreate(xfirst, yfirst, w2, h2);
    pixDestroy(&pix2);

    if (pixadb) {
        pixRenderBoxArb(pixdb1, box1, 2, 255, 0, 0);
        pixRenderBoxArb(pixdb1, box2, 2, 0, 255, 0);
        pixaAddPix(pixadb, pixdb1, L_INSERT);
    }

        /* Select the final result from the two boxes */
    if (select == L_GEOMETRIC_UNION)
        box3 = boxBoundingRegion(box1, box2);
    else if (select == L_GEOMETRIC_INTERSECTION)
        box3 = boxOverlapRegion(box1, box2);
    else if (select == L_LARGEST_AREA)
        box3 = (w1 * h1 >= w2 * h2) ? boxCopy(box1) : boxCopy(box2);
    else  /* select == L_SMALLEST_AREA) */
        box3 = (w1 * h1 <= w2 * h2) ? boxCopy(box1) : boxCopy(box2);
    boxDestroy(&box1);
    boxDestroy(&box2);

        /* Rotate the box 90 degrees ccw if necessary */
    box4 = NULL;
    if (box3) {
        if (dir == L_SCAN_VERTICAL)
            box4 = boxRotateOrth(box3, w, h, 3);
        else
            box4 = boxCopy(box3);
    }

        /* Transform back to global coordinates if %boxs exists */
    box5 = (box4) ? boxTransform(box4, x, y, 1.0, 1.0) : NULL;
    boxDestroy(&box3);
    boxDestroy(&box4);

        /* Debug output */
    if (pixadb) {
        pixdb1 = pixConvertTo8(pixs, 0);
        pixAddConstantGray(pixdb1, 190);
        pixdb2 = pixConvertTo32(pixdb1);
        if (box5) pixRenderBoxArb(pixdb2, box5, 4, 0, 0, 255);
        pixaAddPix(pixadb, pixdb2, L_INSERT);
        res = pixGetXRes(pixs);
        L_INFO("Writing debug files to /tmp/lept/rect/\n", __func__);
        pixaConvertToPdf(pixadb, res, 1.0, L_DEFAULT_ENCODE, 75, NULL,
                        "/tmp/lept/rect/fitrect.pdf");
        pix1 = pixaDisplayTiledAndScaled(pixadb, 32, 800, 1, 0, 40, 2);
        pixWrite("/tmp/lept/rect/fitrect.png", pix1, IFF_PNG);
        pixDestroy(&pix1);
        pixDestroy(&pixdb1);
        pixaDestroy(&pixadb);
    }

    return box5;
}

/*------------------------------------------------------------------*
 *                    Automatic photoinvert for OCR                 *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixAutoPhotoinvert()
 *
 * \param[in]    pixs       any depth, colormap ok
 * \param[in]    thresh     binarization threshold; use 0 for default
 * \param[out]   ppixm      [optional] image regions to be inverted
 * \param[out]   pixadb     [optional] debug; input NULL to skip
 * \return  pixd   1 bpp image to be sent to OCR, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) A 1 bpp image is returned, where pixels in image regions are
 *          photo-inverted.
 *      (2) If there is light text with a dark background, this will
 *          identify the region and photoinvert the pixels there if
 *          there are at least 60% fg pixels in the region.
 *      (3) For debug output, input a (typically empty) %pixadb.
 * </pre>
 */
PIX *
pixAutoPhotoinvert(PIX       *pixs,
                   l_int32    thresh,
                   PIX      **ppixm,
                   PIXA      *pixadb)
{
l_int32    i, n, empty, x, y, w, h;
l_float32  fgfract;
BOX       *box1;
BOXA      *boxa1;
PIX       *pix1, *pix2, *pix3, *pix4, *pix5;

    if (ppixm) *ppixm = NULL;
    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (thresh == 0) thresh = 128;

    if ((pix1 = pixConvertTo1(pixs, thresh)) == NULL)
        return (PIX *)ERROR_PTR("pix1 not made", __func__, NULL);
    if (pixadb) pixaAddPix(pixadb, pix1, L_COPY);

        /* Identify regions for photo-inversion:
         * (1) Start with the halftone mask.
         * (2) Eliminate ordinary text and halftones in the mask.
         * (3) Some regions of inverted text may have been removed in
         *     steps (1) and (2).  Conditionally fill holes in the mask,
         *     but do not fill out to the bounding rect. */
    pix2 = pixGenerateHalftoneMask(pix1, NULL, NULL, pixadb);
    pix3 = pixMorphSequence(pix2, "o15.15 + c25.25", 0);  /* remove noise */
    pix4 = pixFillHolesToBoundingRect(pix3, 1, 0.5, 1.0);
    if (pixadb) {
        pixaAddPix(pixadb, pix2, L_CLONE);
        pixaAddPix(pixadb, pix3, L_CLONE);
        pixaAddPix(pixadb, pix4, L_COPY);
    }
    pixDestroy(&pix2);
    pixDestroy(&pix3);
    pixZero(pix4, &empty);
    if (empty) {
        pixDestroy(&pix4);
        return pix1;
    }

        /* Examine each component and validate the inversion.
         * Require at least 60% of pixels under each component to be FG. */
    boxa1 = pixConnCompBB(pix4, 8);
    n = boxaGetCount(boxa1);
    for (i = 0; i < n; i++) {
        box1 = boxaGetBox(boxa1, i, L_COPY);
        pix5 = pixClipRectangle(pix1, box1, NULL);
        pixForegroundFraction(pix5, &fgfract);
        if (pixadb) lept_stderr("fg fraction: %5.3f\n", fgfract);
        boxGetGeometry(box1, &x, &y, &w, &h);
        if (fgfract < 0.6)  /* erase from the mask */
            pixRasterop(pix4, x, y, w, h, PIX_CLR, NULL, 0, 0);
        pixDestroy(&pix5);
        boxDestroy(&box1);
    }
    boxaDestroy(&boxa1);
    pixZero(pix4, &empty);
    if (empty) {
        pixDestroy(&pix4);
        return pix1;
    }

        /* Combine pixels of the photo-inverted pix with the binarized input */
    pix5 = pixInvert(NULL, pix1);
    pixCombineMasked(pix1, pix5, pix4);

    if (pixadb) {
        pixaAddPix(pixadb, pix5, L_CLONE);
        pixaAddPix(pixadb, pix1, L_COPY);
    }
    pixDestroy(&pix5);
    if (ppixm)
        *ppixm = pix4;
    else
        pixDestroy(&pix4);
    return pix1;
}
