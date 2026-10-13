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

/*
 * jbclass.c
 *
 *     These are functions for unsupervised classification of
 *     collections of connected components -- either characters or
 *     words -- in binary images.  They can be used as image
 *     processing steps in jbig2 compression.
 *
 *     Initialization
 *
 *         JBCLASSER         *jbRankHausInit()      [rank hausdorff encoder]
 *         JBCLASSER         *jbCorrelationInit()   [correlation encoder]
 *         JBCLASSER         *jbCorrelationInitWithoutComponents()  [ditto]
 *         static JBCLASSER  *jbCorrelationInitInternal()
 *
 *     Classify the pages
 *
 *         l_int32     jbAddPages()
 *         l_int32     jbAddPage()
 *         l_int32     jbAddPageComponents()
 *
 *     Rank hausdorff classifier
 *
 *         l_int32     jbClassifyRankHaus()
 *         l_int32     pixHaustest()
 *         l_int32     pixRankHaustest()
 *
 *     Binary correlation classifier
 *
 *         l_int32     jbClassifyCorrelation()
 *
 *     Determine the image components we start with
 *
 *         l_int32     jbGetComponents()
 *         l_int32     pixWordMaskByDilation()
 *         l_int32     pixWordBoxesByDilation()
 *
 *     Build grayscale composites (templates)
 *
 *         PIXA       *jbAccumulateComposites
 *         PIXA       *jbTemplatesFromComposites
 *
 *     Utility functions for Classer
 *
 *         JBCLASSER  *jbClasserCreate()
 *         void        jbClasserDestroy()
 *
 *     Utility functions for Data
 *
 *         JBDATA     *jbDataSave()
 *         void        jbDataDestroy()
 *         l_int32     jbDataWrite()
 *         JBDATA     *jbDataRead()
 *         PIXA       *jbDataRender()
 *         l_int32     jbGetULCorners()
 *         l_int32     jbGetLLCorners()
 *
 *     Static helpers
 *
 *         static JBFINDCTX *findSimilarSizedTemplatesInit()
 *         static l_int32    findSimilarSizedTemplatesNext()
 *         static void       findSimilarSizedTemplatesDestroy()
 *         static l_int32    finalPositioningForAlignment()
 *
 *     Note: this is NOT an implementation of the JPEG jbig2
 *     proposed standard encoder, the specifications for which
 *     can be found at http://www.jpeg.org/jbigpt2.html.
 *     (See below for a full implementation.)
 *     It is an implementation of the lower-level part of an encoder that:
 *
 *        (1) identifies connected components that are going to be used
 *        (2) puts them in similarity classes (this is an unsupervised
 *            classifier), and
 *        (3) stores the result in a simple file format (2 files,
 *            one for templates and one for page/coordinate/template-index
 *            quartets).
 *
 *     An actual implementation of the official jbig2 encoder could
 *     start with parts (1) and (2), and would then compress the quartets
 *     according to the standards requirements (e.g., Huffman or
 *     arithmetic coding of coordinate differences and image templates).
 *
 *     The low-level part of the encoder provided here has the
 *     following useful features:
 *
 *         ~ It is accurate in the identification of templates
 *           and classes because it uses a windowed hausdorff
 *           distance metric.
 *         ~ It is accurate in the placement of the connected
 *           components, doing a two step process of first aligning
 *           the the centroids of the template with those of each instance,
 *           and then making a further correction of up to +- 1 pixel
 *           in each direction to best align the templates.
 *         ~ It is fast because it uses a morphologically based
 *           matching algorithm to implement the hausdorff criterion,
 *           and it selects the patterns that are possible matches
 *           based on their size.
 *
 *     We provide two different matching functions, one using Hausdorff
 *     distance and one using a simple image correlation.
 *     The Hausdorff method sometimes produces better results for the
 *     same number of classes, because it gives a relatively small
 *     effective weight to foreground pixels near the boundary,
 *     and a relatively  large weight to foreground pixels that are
 *     not near the boundary.  By effectively ignoring these boundary
 *     pixels, Hausdorff weighting corresponds better to the expected
 *     probabilities of the pixel values in a scanned image, where the
 *     variations in instances of the same printed character are much
 *     more likely to be in pixels near the boundary.  By contrast,
 *     the correlation method gives equal weight to all foreground pixels.
 *
 *     For best results, use the correlation method.  Correlation takes
 *     the number of fg pixels in the AND of instance and template,
 *     divided by the product of the number of fg pixels in instance
 *     and template.  It compares this with a threshold that, in
 *     general, depends on the fractional coverage of the template.
 *     For heavy text, the threshold is raised above that for light
 *     text,  By using both these parameters (basic threshold and
 *     adjustment factor for text weight), one has more flexibility
 *     and can arrive at the fewest substitution errors, although
 *     this comes at the price of more templates.
 *
 *     The strict Hausdorff scoring is not a rank weighting, because a
 *     single pixel beyond the given distance will cause a match
 *     failure.  A rank Hausdorff is more robust to non-boundary noise,
 *     but it is also more susceptible to confusing components that
 *     should be in different classes.  For implementing a jbig2
 *     application for visually lossless binary image compression,
 *     you have two choices:
 *
 *        (1) use a 3x3 structuring element (size = 3) and a strict
 *            Hausdorff comparison (rank = 1.0 in the rank Hausdorff
 *            function).  This will result in a minimal number of classes,
 *            but confusion of small characters, such as italic and
 *            non-italic lower-case 'o', can still occur.
 *        (2) use the correlation method with a threshold of 0.85
 *            and a weighting factor of about 0.7.  This will result in
 *            a larger number of classes, but should not be confused
 *            either by similar small characters or by extremely
 *            thick sans serif characters, such as in prog/cootoots.png.
 *
 *     As mentioned above, if visual substitution errors must be
 *     avoided, you should use the correlation method.
 *
 *     We provide executables that show how to do the encoding:
 *         prog/jbrankhaus.c
 *         prog/jbcorrelation.c
 *
 *     The basic flow for correlation classification goes as follows,
 *     where specific choices have been made for parameters (Hausdorff
 *     is the same except for initialization):
 *
 *             // Initialize and save data in the classer
 *         JBCLASSER *classer =
 *             jbCorrelationInit(JB_CONN_COMPS, 0, 0, 0.8, 0.7);
 *         SARRAY *safiles = getSortedPathnamesInDirectory(directory,
 *                                                         NULL, 0, 0);
 *         jbAddPages(classer, safiles);
 *
 *             // Save the data in a data structure for serialization,
 *             // and write it into two files.
 *         JBDATA *data = jbDataSave(classer);
 *         jbDataWrite(rootname, data);
 *
 *             // Reconstruct (render) the pages from the encoded data.
 *         PIXA *pixa = jbDataRender(data, FALSE);
 *
 *     Adam Langley has built a jbig2 standards-compliant encoder, the
 *     first one to appear in open source.  You can get this encoder at:
 *          http://www.imperialviolet.org/jbig2.html
 *
 *     It uses arithmetic encoding throughout.  It encodes binary images
 *     losslessly with a single arithmetic coding over the full image.
 *     It also does both lossy and lossless encoding from connected
 *     components, using leptonica to generate the templates representing
 *     each cluster.
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include <math.h>
#include "allheaders.h"
#include "array_internal.h"

#define L_BUF_SIZE 512

    /* For jbClassifyRankHaus(): size of border added around
     * pix of each c.c., to allow further processing.  This
     * should be at least the sum of the MAX_DIFF_HEIGHT
     * (or MAX_DIFF_WIDTH) and one-half the size of the Sel  */
static const l_int32  JB_ADDED_PIXELS = 6;

    /* For pixHaustest(), pixRankHaustest() and pixCorrelationScore():
     * choose these to be 2 or greater */
static const l_int32  MAX_DIFF_WIDTH = 2;  /* use at least 2 */
static const l_int32  MAX_DIFF_HEIGHT = 2;  /* use at least 2 */

    /* In initialization, you have the option to discard components
     * (cc, characters or words) that have either width or height larger
     * than a given size.  This is convenient for jbDataSave(), because
     * the components are placed onto a regular lattice with cell
     * dimension equal to the maximum component size.  The default
     * values are given here.  If you want to save all components,
     * use a sufficiently large set of dimensions. */
static const l_int32  MAX_CONN_COMP_WIDTH = 350;  /* default max cc width */
static const l_int32  MAX_CHAR_COMP_WIDTH = 350;  /* default max char width */
static const l_int32  MAX_WORD_COMP_WIDTH = 1000;  /* default max word width */
static const l_int32  MAX_COMP_HEIGHT = 120;  /* default max component height */

    /* This stores the state of a state machine which fetches
     * similar sized templates */
struct JbFindTemplatesState
{
    JBCLASSER       *classer;    /* classer                               */
    l_int32          w;          /* desired width                         */
    l_int32          h;          /* desired height                        */
    l_int32          i;          /* index into two_by_two step array      */
    L_DNA           *dna;        /* current number array                  */
    l_int32          n;          /* current element of dna                */
};
typedef struct JbFindTemplatesState JBFINDCTX;

    /* Static initialization function */
static JBCLASSER * jbCorrelationInitInternal(l_int32 components,
                       l_int32 maxwidth, l_int32 maxheight, l_float32 thresh,
                       l_float32 weightfactor, l_int32 keep_components);

    /* Static helper functions */
static JBFINDCTX * findSimilarSizedTemplatesInit(JBCLASSER *classer, PIX *pixs);
static l_int32 findSimilarSizedTemplatesNext(JBFINDCTX *context);
static void findSimilarSizedTemplatesDestroy(JBFINDCTX **pcontext);
static l_int32 finalPositioningForAlignment(PIX *pixs, l_int32 x, l_int32 y,
                             l_int32 idelx, l_int32 idely, PIX *pixt,
                             l_int32 *sumtab, l_int32 *pdx, l_int32 *pdy);

#ifndef NO_CONSOLE_IO
#define  DEBUG_CORRELATION_SCORE   0
#endif  /* ~NO_CONSOLE_IO */

/*----------------------------------------------------------------------*
 *                            Initialization                            *
 *----------------------------------------------------------------------*/
/*!
 * \brief   jbRankHausInit()
 *
 * \param[in]  components  JB_CONN_COMPS, JB_CHARACTERS, JB_WORDS
 * \param[in]  maxwidth    of component; use 0 for default
 * \param[in]  maxheight   of component; use 0 for default
 * \param[in]  size        of square structuring element; 2, representing
 *                         2x2 sel, is necessary for reasonable accuracy of
 *                         small components; combine this with rank ~ 0.97
 *                         to avoid undue class expansion
 * \param[in]    rank      rank val of match, each way; in [0.5 - 1.0];
 *                         when using size = 2, 0.97 is a reasonable value
 * \return  jbclasser if OK; NULL on error
 */
JBCLASSER *
jbRankHausInit(l_int32    components,
               l_int32    maxwidth,
               l_int32    maxheight,
               l_int32    size,
               l_float32  rank)
{
JBCLASSER  *classer;

    if (components != JB_CONN_COMPS && components != JB_CHARACTERS &&
        components != JB_WORDS)
        return (JBCLASSER *)ERROR_PTR("invalid components", __func__, NULL);
    if (size < 1 || size > 10)
        return (JBCLASSER *)ERROR_PTR("size not reasonable", __func__, NULL);
    if (rank < 0.5 || rank > 1.0)
        return (JBCLASSER *)ERROR_PTR("rank not in [0.5-1.0]", __func__, NULL);
    if (maxwidth == 0) {
        if (components == JB_CONN_COMPS)
            maxwidth = MAX_CONN_COMP_WIDTH;
        else if (components == JB_CHARACTERS)
            maxwidth = MAX_CHAR_COMP_WIDTH;
        else  /* JB_WORDS */
            maxwidth = MAX_WORD_COMP_WIDTH;
    }
    if (maxheight == 0)
        maxheight = MAX_COMP_HEIGHT;

    if ((classer = jbClasserCreate(JB_RANKHAUS, components)) == NULL)
        return (JBCLASSER *)ERROR_PTR("classer not made", __func__, NULL);
    classer->maxwidth = maxwidth;
    classer->maxheight = maxheight;
    classer->sizehaus = size;
    classer->rankhaus = rank;
    classer->dahash = l_dnaHashCreate(5507, 4);  /* 5507 is prime */
    classer->keep_pixaa = 1;  /* keep all components in pixaa */
    return classer;
}


/*!
 * \brief   jbCorrelationInit()
 *
 * \param[in]  components    JB_CONN_COMPS, JB_CHARACTERS, JB_WORDS
 * \param[in]  maxwidth      of component; use 0 for default
 * \param[in]  maxheight     of component; use 0 for default
 * \param[in]  thresh        value for correlation score: in [0.4 - 0.98]
 * \param[in]  weightfactor  corrects thresh for thick characters [0.0 - 1.0]
 * \return  jbclasser if OK; NULL on error
 *
 * <pre>
 * Notes:
 *      (1) For scanned text, suggested input values are:
 *            thresh ~ [0.8 - 0.85]
 *            weightfactor ~ [0.5 - 0.6]
 *      (2) For electronically generated fonts (e.g., rasterized pdf),
 *          a very high thresh (e.g., 0.95) will not cause a significant
 *          increase in the number of classes.
 * </pre>
 */
JBCLASSER *
jbCorrelationInit(l_int32    components,
                  l_int32    maxwidth,
                  l_int32    maxheight,
                  l_float32  thresh,
                  l_float32  weightfactor)
{
    return jbCorrelationInitInternal(components, maxwidth, maxheight, thresh,
                                     weightfactor, 1);
}

/*!
 * \brief   jbCorrelationInitWithoutComponents()
 *
 * \param[in]  components    JB_CONN_COMPS, JB_CHARACTERS, JB_WORDS
 * \param[in]  maxwidth      of component; use 0 for default
 * \param[in]  maxheight     of component; use 0 for default
 * \param[in]  thresh value  for correlation score: in [0.4 - 0.98]
 * \param[in]  weightfactor  corrects thresh for thick characters [0.0 - 1.0]
 * \return  jbclasser if OK; NULL on error
 *
 * <pre>
 * Notes:
 *      Acts the same as jbCorrelationInit(), but the resulting
 *      object doesn't keep a list of all the components.
 * </pre>
 */
JBCLASSER *
jbCorrelationInitWithoutComponents(l_int32    components,
                                   l_int32    maxwidth,
                                   l_int32    maxheight,
                                   l_float32  thresh,
                                   l_float32  weightfactor)
{
    return jbCorrelationInitInternal(components, maxwidth, maxheight, thresh,
                                     weightfactor, 0);
}


static JBCLASSER *
jbCorrelationInitInternal(l_int32    components,
                          l_int32    maxwidth,
                          l_int32    maxheight,
                          l_float32  thresh,
                          l_float32  weightfactor,
                          l_int32    keep_components)
{
JBCLASSER  *classer;

    if (components != JB_CONN_COMPS && components != JB_CHARACTERS &&
        components != JB_WORDS)
        return (JBCLASSER *)ERROR_PTR("invalid components", __func__, NULL);
    if (thresh < 0.4 || thresh > 0.98)
        return (JBCLASSER *)ERROR_PTR("thresh not in range [0.4 - 0.98]",
                __func__, NULL);
    if (weightfactor < 0.0 || weightfactor > 1.0)
        return (JBCLASSER *)ERROR_PTR("weightfactor not in range [0.0 - 1.0]",
                __func__, NULL);
    if (maxwidth == 0) {
        if (components == JB_CONN_COMPS)
            maxwidth = MAX_CONN_COMP_WIDTH;
        else if (components == JB_CHARACTERS)
            maxwidth = MAX_CHAR_COMP_WIDTH;
        else  /* JB_WORDS */
            maxwidth = MAX_WORD_COMP_WIDTH;
    }
    if (maxheight == 0)
        maxheight = MAX_COMP_HEIGHT;


    if ((classer = jbClasserCreate(JB_CORRELATION, components)) == NULL)
        return (JBCLASSER *)ERROR_PTR("classer not made", __func__, NULL);
    classer->maxwidth = maxwidth;
    classer->maxheight = maxheight;
    classer->thresh = thresh;
    classer->weightfactor = weightfactor;
    classer->dahash = l_dnaHashCreate(5507, 4);  /* 5507 is prime */
    classer->keep_pixaa = keep_components;
    return classer;
}


/*----------------------------------------------------------------------*
 *                       Classify the pages                             *
 *----------------------------------------------------------------------*/
/*!
 * \brief   jbAddPages()
 *
 * \param[in]    jbclasser
 * \param[in]    safiles      of page image file names
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) jbclasser makes a copy of the array of file names.
 *      (2) The caller is still responsible for destroying the input array.
 * </pre>
 */
l_ok
jbAddPages(JBCLASSER  *classer,
           SARRAY     *safiles)
{
l_int32  i, nfiles;
char    *fname;
PIX     *pix;

    if (!classer)
        return ERROR_INT("classer not defined", __func__, 1);
    if (!safiles)
        return ERROR_INT("safiles not defined", __func__, 1);

    classer->safiles = sarrayCopy(safiles);
    nfiles = sarrayGetCount(safiles);
    for (i = 0; i < nfiles; i++) {
        fname = sarrayGetString(safiles, i, L_NOCOPY);
        if ((pix = pixRead(fname)) == NULL) {
            L_WARNING("image file %d not read\n", __func__, i);
            continue;
        }
        if (pixGetDepth(pix) != 1) {
            L_WARNING("image file %d not 1 bpp\n", __func__, i);
            continue;
        }
        jbAddPage(classer, pix);
        pixDestroy(&pix);
    }

    return 0;
}


/*!
 * \brief   jbAddPage()
 *
 * \param[in]    jbclasser
 * \param[in]    pixs      input page
 * \return  0 if OK; 1 on error
 */
l_ok
jbAddPage(JBCLASSER  *classer,
          PIX        *pixs)
{
BOXA  *boxas;
PIXA  *pixas;

    if (!classer)
        return ERROR_INT("classer not defined", __func__, 1);
    if (!pixs || pixGetDepth(pixs) != 1)
        return ERROR_INT("pixs not defined or not 1 bpp", __func__, 1);

    classer->w = pixGetWidth(pixs);
    classer->h = pixGetHeight(pixs);

        /* Get the appropriate components and their bounding boxes */
    if (jbGetComponents(pixs, classer->components, classer->maxwidth,
                        classer->maxheight, &boxas, &pixas)) {
        return ERROR_INT("components not made", __func__, 1);
    }

    jbAddPageComponents(classer, pixs, boxas, pixas);
    boxaDestroy(&boxas);
    pixaDestroy(&pixas);
    return 0;
}


/*!
 * \brief   jbAddPageComponents()
 *
 * \param[in]    jbclasser
 * \param[in]    pixs      input page
 * \param[in]    boxas     b.b. of components for this page
 * \param[in]    pixas     components for this page
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If there are no components on the page, we don't require input
 *          of empty boxas or pixas, although that's the typical situation.
 * </pre>
 */
l_ok
jbAddPageComponents(JBCLASSER  *classer,
                    PIX        *pixs,
                    BOXA       *boxas,
                    PIXA       *pixas)
{
l_int32  n;

    if (!classer)
        return ERROR_INT("classer not defined", __func__, 1);
    if (!pixs)
        return ERROR_INT("pix not defined", __func__, 1);

        /* Test for no components on the current page.  Always update the
         * number of pages processed, even if nothing is on it. */
    if (!boxas || !pixas || (boxaGetCount(boxas) == 0)) {
        classer->npages++;
        return 0;
    }

        /* Get classes.  For hausdorff, it uses a specified size of
         * structuring element and specified rank.  For correlation,
         * it uses a specified threshold. */
    if (classer->method == JB_RANKHAUS) {
        if (jbClassifyRankHaus(classer, boxas, pixas))
            return ERROR_INT("rankhaus classification failed", __func__, 1);
    } else {  /* classer->method == JB_CORRELATION */
        if (jbClassifyCorrelation(classer, boxas, pixas))
            return ERROR_INT("correlation classification failed", __func__, 1);
    }

        /* Find the global UL corners, adjusted for each instance so
         * that the class template and instance will have their
         * centroids in the same place.  Then the template can be
         * used to replace the instance. */
    if (jbGetULCorners(classer, pixs, boxas))
        return ERROR_INT("UL corners not found", __func__, 1);

        /* Update total component counts and number of pages processed. */
    n = boxaGetCount(boxas);
    classer->baseindex += n;
    numaAddNumber(classer->nacomps, (l_float32)n);
    classer->npages++;
    return 0;
}


/*----------------------------------------------------------------------*
 *         Classification using windowed rank hausdorff metric          *
 *----------------------------------------------------------------------*/
/*!
 * \brief   jbClassifyRankHaus()
 *
 * \param[in]    jbclasser
 * \param[in]    boxa      new components for classification
 * \param[in]    pixas     new components for classification
 * \return  0 if OK; 1 on error
 */
l_ok
jbClassifyRankHaus(JBCLASSER  *classer,
                   BOXA       *boxa,
                   PIXA       *pixas)
{
l_int32     n, nt, i, wt, ht, iclass, size, found, testval;
l_int32     npages, area1, area3;
l_int32    *tab8;
l_float32   rank, x1, y1, x2, y2;
BOX        *box;
NUMA       *naclass, *napage;
NUMA       *nafg;   /* fg area of all instances */
NUMA       *nafgt;  /* fg area of all templates */
JBFINDCTX  *findcontext;
L_DNAHASH  *dahash;
PIX        *pix, *pix1, *pix2, *pix3, *pix4;
PIXA       *pixa, *pixa1, *pixa2, *pixat, *pixatd;
PIXAA      *pixaa;
PTA        *pta, *ptac, *ptact;
SEL        *sel;

    if (!classer)
        return ERROR_INT("classer not defined", __func__, 1);
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);
    if (!pixas)
        return ERROR_INT("pixas not defined", __func__, 1);
    if ((n = pixaGetCount(pixas)) == 0)
        return ERROR_INT("pixas is empty", __func__, 1);
    if ((nafg = pixaCountPixels(pixas)) == NULL)  /* areas for this page */
        return ERROR_INT("fg counting failed", __func__, 1);

    npages = classer->npages;
    size = classer->sizehaus;
    sel = selCreateBrick(size, size, size / 2, size / 2, SEL_HIT);

        /* Generate the bordered pixa, with and without dilation.
         * pixa1 and pixa2 contain all the input components. */
    pixa1 = pixaCreate(n);
    pixa2 = pixaCreate(n);
    for (i = 0; i < n; i++) {
        pix = pixaGetPix(pixas, i, L_CLONE);
        pix1 = pixAddBorderGeneral(pix, JB_ADDED_PIXELS, JB_ADDED_PIXELS,
                    JB_ADDED_PIXELS, JB_ADDED_PIXELS, 0);
        pix2 = pixDilate(NULL, pix1, sel);
        pixaAddPix(pixa1, pix1, L_INSERT);   /* un-dilated */
        pixaAddPix(pixa2, pix2, L_INSERT);   /* dilated */
        pixDestroy(&pix);
    }

        /* Get the centroids of all the bordered images.
         * These are relative to the UL corner of each (bordered) pix.  */
    pta = pixaCentroids(pixa1);  /* centroids for this page; use here */
    ptac = classer->ptac;  /* holds centroids of components up to this page */
    ptaJoin(ptac, pta, 0, -1);  /* save centroids of all components */
    ptact = classer->ptact;  /* holds centroids of templates */

        /* Use these to save the class and page of each component. */
    naclass = classer->naclass;
    napage = classer->napage;

        /* Store the unbordered pix in a pixaa, in a hierarchical
         * set of arrays.  There is one pixa for each class,
         * and the pix in each pixa are all the instances found
         * of that class.  This is actually more than one would need
         * for a jbig2 encoder, but there are two reasons to keep
         * them around: (1) the set of instances for each class
         * can be used to make an improved binary (or, better,
         * a grayscale) template, rather than simply using the first
         * one in the set; (2) we can investigate the failures
         * of the classifier.  This pixaa grows as we process
         * successive pages. */
    pixaa = classer->pixaa;

        /* arrays to store class exemplars (templates) */
    pixat = classer->pixat;   /* un-dilated */
    pixatd = classer->pixatd;   /* dilated */

        /* Fill up the pixaa tree with the template exemplars as
         * the first pix in each pixa.  As we add each pix,
         * we also add the associated box to the pixa.
         * We also keep track of the centroid of each pix,
         * and use the difference between centroids (of the
         * pix with the exemplar we are checking it with)
         * to align the two when checking that the Hausdorff
         * distance does not exceed a threshold.
         * The threshold is set by the Sel used for dilating.
         * For example, a 3x3 brick, sel_3, corresponds to a
         * Hausdorff distance of 1.  In general, for an NxN brick,
         * with N odd, corresponds to a Hausdorff distance of (N - 1)/2.
         * It turns out that we actually need to use a sel of size 2x2
         * to avoid small bad components when there is a halftone image
         * from which components can be chosen.
         * The larger the Sel you use, the fewer the number of classes,
         * and the greater the likelihood of putting semantically
         * different objects in the same class.  For simplicity,
         * we do this separately for the case of rank == 1.0 (exact
         * match within the Hausdorff distance) and rank < 1.0.  */
    rank = classer->rankhaus;
    dahash = classer->dahash;
    if (rank == 1.0) {
        for (i = 0; i < n; i++) {
            pix1 = pixaGetPix(pixa1, i, L_CLONE);
            pix2 = pixaGetPix(pixa2, i, L_CLONE);
            ptaGetPt(pta, i, &x1, &y1);
            nt = pixaGetCount(pixat);  /* number of templates */
            found = FALSE;
            findcontext = findSimilarSizedTemplatesInit(classer, pix1);
            while ((iclass = findSimilarSizedTemplatesNext(findcontext)) > -1) {
                    /* Find score for this template */
                pix3 = pixaGetPix(pixat, iclass, L_CLONE);
                pix4 = pixaGetPix(pixatd, iclass, L_CLONE);
                ptaGetPt(ptact, iclass, &x2, &y2);
                testval = pixHaustest(pix1, pix2, pix3, pix4, x1 - x2, y1 - y2,
                                      MAX_DIFF_WIDTH, MAX_DIFF_HEIGHT);
                pixDestroy(&pix3);
                pixDestroy(&pix4);
                if (testval == 1) {
                    found = TRUE;
                    numaAddNumber(naclass, (l_float32)iclass);
                    numaAddNumber(napage, (l_float32)npages);
                    if (classer->keep_pixaa) {
                        pixa = pixaaGetPixa(pixaa, iclass, L_CLONE);
                        pix = pixaGetPix(pixas, i, L_CLONE);
                        pixaAddPix(pixa, pix, L_INSERT);
                        box = boxaGetBox(boxa, i, L_CLONE);
                        pixaAddBox(pixa, box, L_INSERT);
                        pixaDestroy(&pixa);
                    }
                    break;
                }
            }
            findSimilarSizedTemplatesDestroy(&findcontext);
            if (found == FALSE) {  /* new class */
                numaAddNumber(naclass, (l_float32)nt);
                numaAddNumber(napage, (l_float32)npages);
                pixa = pixaCreate(0);
                pix = pixaGetPix(pixas, i, L_CLONE);  /* unbordered instance */
                pixaAddPix(pixa, pix, L_INSERT);
                wt = pixGetWidth(pix);
                ht = pixGetHeight(pix);
                l_dnaHashAdd(dahash, (l_uint64)ht * wt, nt);
                box = boxaGetBox(boxa, i, L_CLONE);
                pixaAddBox(pixa, box, L_INSERT);
                pixaaAddPixa(pixaa, pixa, L_INSERT);  /* unbordered instance */
                ptaAddPt(ptact, x1, y1);
                pixaAddPix(pixat, pix1, L_INSERT);  /* bordered template */
                pixaAddPix(pixatd, pix2, L_INSERT);  /* bordered dil template */
            } else {  /* don't save them */
                pixDestroy(&pix1);
                pixDestroy(&pix2);
            }
        }
    } else {  /* rank < 1.0 */
        nafgt = classer->nafgt;
        tab8 = makePixelSumTab8();
        for (i = 0; i < n; i++) {   /* all instances on this page */
            pix1 = pixaGetPix(pixa1, i, L_CLONE);
            numaGetIValue(nafg, i, &area1);
            pix2 = pixaGetPix(pixa2, i, L_CLONE);
            ptaGetPt(pta, i, &x1, &y1);   /* use pta for this page */
            nt = pixaGetCount(pixat);  /* number of templates */
            found = FALSE;
            findcontext = findSimilarSizedTemplatesInit(classer, pix1);
            while ((iclass = findSimilarSizedTemplatesNext(findcontext)) > -1) {
                    /* Find score for this template */
                pix3 = pixaGetPix(pixat, iclass, L_CLONE);
                numaGetIValue(nafgt, iclass, &area3);
                pix4 = pixaGetPix(pixatd, iclass, L_CLONE);
                ptaGetPt(ptact, iclass, &x2, &y2);
                testval = pixRankHaustest(pix1, pix2, pix3, pix4,
                                          x1 - x2, y1 - y2,
                                          MAX_DIFF_WIDTH, MAX_DIFF_HEIGHT,
                                          area1, area3, rank, tab8);
                pixDestroy(&pix3);
                pixDestroy(&pix4);
                if (testval == 1) {  /* greedy match; take the first */
                    found = TRUE;
                    numaAddNumber(naclass, (l_float32)iclass);
                    numaAddNumber(napage, (l_float32)npages);
                    if (classer->keep_pixaa) {
                        pixa = pixaaGetPixa(pixaa, iclass, L_CLONE);
                        pix = pixaGetPix(pixas, i, L_CLONE);
                        pixaAddPix(pixa, pix, L_INSERT);
                        box = boxaGetBox(boxa, i, L_CLONE);
                        pixaAddBox(pixa, box, L_INSERT);
                        pixaDestroy(&pixa);
                    }
                    break;
                }
            }
            findSimilarSizedTemplatesDestroy(&findcontext);
            if (found == FALSE) {  /* new class */
                numaAddNumber(naclass, (l_float32)nt);
                numaAddNumber(napage, (l_float32)npages);
                pixa = pixaCreate(0);
                pix = pixaGetPix(pixas, i, L_CLONE);  /* unbordered instance */
                pixaAddPix(pixa, pix, L_INSERT);
                wt = pixGetWidth(pix);
                ht = pixGetHeight(pix);
                l_dnaHashAdd(dahash, (l_uint64)ht * wt, nt);
                box = boxaGetBox(boxa, i, L_CLONE);
                pixaAddBox(pixa, box, L_INSERT);
                pixaaAddPixa(pixaa, pixa, L_INSERT);  /* unbordered instance */
                ptaAddPt(ptact, x1, y1);
                pixaAddPix(pixat, pix1, L_INSERT);  /* bordered template */
                pixaAddPix(pixatd, pix2, L_INSERT);  /* ditto */
                numaAddNumber(nafgt, (l_float32)area1);
            } else {  /* don't save them */
                pixDestroy(&pix1);
                pixDestroy(&pix2);
            }
        }
        LEPT_FREE(tab8);
    }
    classer->nclass = pixaGetCount(pixat);

    numaDestroy(&nafg);
    ptaDestroy(&pta);
    pixaDestroy(&pixa1);
    pixaDestroy(&pixa2);
    selDestroy(&sel);
    return 0;
}


/*!
 * \brief   pixHaustest()
 *
 * \param[in]  pix1      new pix, not dilated
 * \param[in]  pix2      new pix, dilated
 * \param[in]  pix3      exemplar pix, not dilated
 * \param[in]  pix4      exemplar pix, dilated
 * \param[in]  delx      x comp of centroid difference
 * \param[in]  dely      y comp of centroid difference
 * \param[in]  maxdiffw  max width difference of pix1 and pix2
 * \param[in]  maxdiffh  max height difference of pix1 and pix2
 * \return  0 FALSE) if no match, 1 (TRUE if the new
 *              pix is in the same class as the exemplar.
 *
 * <pre>
 * Notes:
 *  We check first that the two pix are roughly
 *  the same size.  Only if they meet that criterion do
 *  we compare the bitmaps.  The Hausdorff is a 2-way
 *  check.  The centroid difference is used to align the two
 *  images to the nearest integer for each of the checks.
 *  These check that the dilated image of one contains
 *  ALL the pixels of the undilated image of the other.
 *  Checks are done in both direction.  A single pixel not
 *  contained in either direction results in failure of the test.
 * </pre>
 */
l_int32
pixHaustest(PIX       *pix1,
            PIX       *pix2,
            PIX       *pix3,
            PIX       *pix4,
            l_float32  delx,   /* x(1) - x(3) */
            l_float32  dely,   /* y(1) - y(3) */
            l_int32    maxdiffw,
            l_int32    maxdiffh)
{
l_int32  wi, hi, wt, ht, delw, delh, idelx, idely, boolmatch;
PIX     *pixt;

        /* Eliminate possible matches based on size difference */
    wi = pixGetWidth(pix1);
    hi = pixGetHeight(pix1);
    wt = pixGetWidth(pix3);
    ht = pixGetHeight(pix3);
    delw = L_ABS(wi - wt);
    if (delw > maxdiffw)
        return FALSE;
    delh = L_ABS(hi - ht);
    if (delh > maxdiffh)
        return FALSE;

        /* Round difference in centroid location to nearest integer;
         * use this as a shift when doing the matching. */
    if (delx >= 0)
        idelx = (l_int32)(delx + 0.5);
    else
        idelx = (l_int32)(delx - 0.5);
    if (dely >= 0)
        idely = (l_int32)(dely + 0.5);
    else
        idely = (l_int32)(dely - 0.5);

        /*  Do 1-direction hausdorff, checking that every pixel in pix1
         *  is within a dilation distance of some pixel in pix3.  Namely,
         *  that pix4 entirely covers pix1:
         *       pixt = pixSubtract(NULL, pix1, pix4), including shift
         *  where pixt has no ON pixels.  */
    pixt = pixCreateTemplate(pix1);
    pixRasterop(pixt, 0, 0, wi, hi, PIX_SRC, pix1, 0, 0);
    pixRasterop(pixt, idelx, idely, wi, hi, PIX_DST & PIX_NOT(PIX_SRC),
                pix4, 0, 0);
    pixZero(pixt, &boolmatch);
    if (boolmatch == 0) {
        pixDestroy(&pixt);
        return FALSE;
    }

        /*  Do 1-direction hausdorff, checking that every pixel in pix3
         *  is within a dilation distance of some pixel in pix1.  Namely,
         *  that pix2 entirely covers pix3:
         *      pixSubtract(pixt, pix3, pix2), including shift
         *  where pixt has no ON pixels. */
    pixRasterop(pixt, idelx, idely, wt, ht, PIX_SRC, pix3, 0, 0);
    pixRasterop(pixt, 0, 0, wt, ht, PIX_DST & PIX_NOT(PIX_SRC), pix2, 0, 0);
    pixZero(pixt, &boolmatch);
    pixDestroy(&pixt);
    return boolmatch;
}


/*!
 * \brief   pixRankHaustest()
 *
 * \param[in]   pix1      new pix, not dilated
 * \param[in]   pix2      new pix, dilated
 * \param[in]   pix3      exemplar pix, not dilated
 * \param[in]   pix4      exemplar pix, dilated
 * \param[in]   delx      x comp of centroid difference
 * \param[in]   dely      y comp of centroid difference
 * \param[in]   maxdiffw  max width difference of pix1 and pix2
 * \param[in]   maxdiffh  max height difference of pix1 and pix2
 * \param[in]   area1     fg pixels in pix1
 * \param[in]   area3     fg pixels in pix3
 * \param[in]   rank      rank value of test, each way
 * \param[in]   tab8      table of pixel sums for byte
 * \return  0 FALSE) if no match, 1 (TRUE if the new
 *                 pix is in the same class as the exemplar.
 *
 * <pre>
 * Notes:
 *  We check first that the two pix are roughly
 *  the same size.  Only if they meet that criterion do
 *  we compare the bitmaps.  We convert the rank value to
 *  a number of pixels by multiplying the rank fraction by the number
 *  of pixels in the undilated image.  The Hausdorff is a 2-way
 *  check.  The centroid difference is used to align the two
 *  images to the nearest integer for each of the checks.
 *  The rank hausdorff checks that the dilated image of one
 *  contains the rank fraction of the pixels of the undilated
 *  image of the other.   Checks are done in both direction.
 *  Failure of the test in either direction results in failure
 *  of the test.
 * </pre>
 */
l_int32
pixRankHaustest(PIX       *pix1,
                PIX       *pix2,
                PIX       *pix3,
                PIX       *pix4,
                l_float32  delx,   /* x(1) - x(3) */
                l_float32  dely,   /* y(1) - y(3) */
                l_int32    maxdiffw,
                l_int32    maxdiffh,
                l_int32    area1,
                l_int32    area3,
                l_float32  rank,
                l_int32   *tab8)
{
l_int32  wi, hi, wt, ht, delw, delh, idelx, idely, boolmatch;
l_int32  thresh1, thresh3;
PIX     *pixt;

        /* Eliminate possible matches based on size difference */
    wi = pixGetWidth(pix1);
    hi = pixGetHeight(pix1);
    wt = pixGetWidth(pix3);
    ht = pixGetHeight(pix3);
    delw = L_ABS(wi - wt);
    if (delw > maxdiffw)
        return FALSE;
    delh = L_ABS(hi - ht);
    if (delh > maxdiffh)
        return FALSE;

        /* Upper bounds in remaining pixels for allowable match */
    thresh1 = (l_int32)(area1 * (1. - rank) + 0.5);
    thresh3 = (l_int32)(area3 * (1. - rank) + 0.5);

        /* Round difference in centroid location to nearest integer;
         * use this as a shift when doing the matching. */
    if (delx >= 0)
        idelx = (l_int32)(delx + 0.5);
    else
        idelx = (l_int32)(delx - 0.5);
    if (dely >= 0)
        idely = (l_int32)(dely + 0.5);
    else
        idely = (l_int32)(dely - 0.5);

        /*  Do 1-direction hausdorff, checking that every pixel in pix1
         *  is within a dilation distance of some pixel in pix3.  Namely,
         *  that pix4 entirely covers pix1:
         *       pixt = pixSubtract(NULL, pix1, pix4), including shift
         *  where pixt has no ON pixels.  */
    pixt = pixCreateTemplate(pix1);
    pixRasterop(pixt, 0, 0, wi, hi, PIX_SRC, pix1, 0, 0);
    pixRasterop(pixt, idelx, idely, wi, hi, PIX_DST & PIX_NOT(PIX_SRC),
                pix4, 0, 0);
    pixThresholdPixelSum(pixt, thresh1, &boolmatch, tab8);
    if (boolmatch == 1) { /* above thresh1 */
        pixDestroy(&pixt);
        return FALSE;
    }

        /*  Do 1-direction hausdorff, checking that every pixel in pix3
         *  is within a dilation distance of some pixel in pix1.  Namely,
         *  that pix2 entirely covers pix3:
         *      pixSubtract(pixt, pix3, pix2), including shift
         *  where pixt has no ON pixels. */
    pixRasterop(pixt, idelx, idely, wt, ht, PIX_SRC, pix3, 0, 0);
    pixRasterop(pixt, 0, 0, wt, ht, PIX_DST & PIX_NOT(PIX_SRC), pix2, 0, 0);
    pixThresholdPixelSum(pixt, thresh3, &boolmatch, tab8);
    pixDestroy(&pixt);
    if (boolmatch == 1)  /* above thresh3 */
        return FALSE;
    else
        return TRUE;
}


/*----------------------------------------------------------------------*
 *            Classification using windowed correlation score           *
 *----------------------------------------------------------------------*/
/*!
 * \brief   jbClassifyCorrelation()
 *
 * \param[in]   jbclasser
 * \param[in]   boxa      new components for classification
 * \param[in]   pixas     new components for classification
 * \return  0 if OK; 1 on error
 */
l_ok
jbClassifyCorrelation(JBCLASSER  *classer,
                      BOXA       *boxa,
                      PIXA       *pixas)
{
l_int32     n, nt, i, iclass, wt, ht, found, area, area1, area2, npages,
            overthreshold;
l_int32    *sumtab, *centtab;
l_uint32   *row, word;
l_float32   x1, y1, x2, y2, xsum, ysum;
l_float32   thresh, weight, threshold;
BOX        *box;
NUMA       *naclass, *napage;
NUMA       *nafgt;   /* fg area of all templates */
NUMA       *naarea;   /* w * h area of all templates */
JBFINDCTX  *findcontext;
L_DNAHASH  *dahash;
PIX        *pix, *pix1, *pix2;
PIXA       *pixa, *pixa1, *pixat;
PIXAA      *pixaa;
PTA        *pta, *ptac, *ptact;
l_int32    *pixcts;  /* pixel counts of each pixa */
l_int32   **pixrowcts;  /* row-by-row pixel counts of each pixa */
l_int32     x, y, rowcount, downcount, wpl;
l_uint8     byte;

    if (!classer)
        return ERROR_INT("classer not found", __func__, 1);
    if (!boxa)
        return ERROR_INT("boxa not found", __func__, 1);
    if (!pixas)
        return ERROR_INT("pixas not found", __func__, 1);

    npages = classer->npages;

        /* Generate the bordered pixa, which contains all the the
         * input components.  This will not be saved.   */
    if ((n = pixaGetCount(pixas)) == 0) {
        L_WARNING("pixas is empty\n", __func__);
        return 0;
    }
    pixa1 = pixaCreate(n);
    for (i = 0; i < n; i++) {
        pix = pixaGetPix(pixas, i, L_CLONE);
        pix1 = pixAddBorderGeneral(pix, JB_ADDED_PIXELS, JB_ADDED_PIXELS,
                    JB_ADDED_PIXELS, JB_ADDED_PIXELS, 0);
        pixaAddPix(pixa1, pix1, L_INSERT);
        pixDestroy(&pix);
    }

        /* Use these to save the class and page of each component. */
    naclass = classer->naclass;
    napage = classer->napage;

        /* Get the number of fg pixels in each component.  */
    nafgt = classer->nafgt;    /* holds fg areas of the templates */
    sumtab = makePixelSumTab8();

    pixcts = (l_int32 *)LEPT_CALLOC(n, sizeof(*pixcts));
    pixrowcts = (l_int32 **)LEPT_CALLOC(n, sizeof(*pixrowcts));
    centtab = makePixelCentroidTab8();

        /* Count the "1" pixels in each row of the pix in pixa1; this
         * allows pixCorrelationScoreThresholded to abort early if a match
         * is impossible.  This loop merges three calculations: the total
         * number of "1" pixels, the number of "1" pixels in each row, and
         * the centroid.  The centroids are relative to the UL corner of
         * each (bordered) pix.  The pixrowcts[i][y] are the total number
         * of fg pixels in pixa[i] below row y. */
    pta = ptaCreate(n);
    for (i = 0; i < n; i++) {
        pix = pixaGetPix(pixa1, i, L_CLONE);
        pixrowcts[i] = (l_int32 *)LEPT_CALLOC(pixGetHeight(pix),
                                         sizeof(**pixrowcts));
        xsum = 0;
        ysum = 0;
        wpl = pixGetWpl(pix);
        row = pixGetData(pix) + (pixGetHeight(pix) - 1) * wpl;
        downcount = 0;
        for (y = pixGetHeight(pix) - 1; y >= 0; y--, row -= wpl) {
            pixrowcts[i][y] = downcount;
            rowcount = 0;
            for (x = 0; x < wpl; x++) {
                word = row[x];
                byte = word & 0xff;
                rowcount += sumtab[byte];
                xsum += centtab[byte] + (x * 32 + 24) * sumtab[byte];
                byte = (word >> 8) & 0xff;
                rowcount += sumtab[byte];
                xsum += centtab[byte] + (x * 32 + 16) * sumtab[byte];
                byte = (word >> 16) & 0xff;
                rowcount += sumtab[byte];
                xsum += centtab[byte] + (x * 32 + 8) * sumtab[byte];
                byte = (word >> 24) & 0xff;
                rowcount += sumtab[byte];
                xsum += centtab[byte] + x * 32 * sumtab[byte];
            }
            downcount += rowcount;
            ysum += rowcount * y;
        }
        pixcts[i] = downcount;
        if (downcount > 0) {
            ptaAddPt(pta,
                 xsum / (l_float32)downcount, ysum / (l_float32)downcount);
        } else {  /* no pixels; shouldn't happen */
            L_ERROR("downcount == 0 !\n", __func__);
            ptaAddPt(pta, pixGetWidth(pix) / 2, pixGetHeight(pix) / 2);
        }
        pixDestroy(&pix);
    }

    ptac = classer->ptac;  /* holds centroids of components up to this page */
    ptaJoin(ptac, pta, 0, -1);  /* save centroids of all components */
    ptact = classer->ptact;  /* holds centroids of templates */

    /* Store the unbordered pix in a pixaa, in a hierarchical
     * set of arrays.  There is one pixa for each class,
     * and the pix in each pixa are all the instances found
     * of that class.  This is actually more than one would need
     * for a jbig2 encoder, but there are two reasons to keep
     * them around: (1) the set of instances for each class
     * can be used to make an improved binary (or, better,
     * a grayscale) template, rather than simply using the first
     * one in the set; (2) we can investigate the failures
     * of the classifier.  This pixaa grows as we process
     * successive pages. */
    pixaa = classer->pixaa;

        /* Array to store class exemplars */
    pixat = classer->pixat;

        /* Fill up the pixaa tree with the template exemplars as
         * the first pix in each pixa.  As we add each pix,
         * we also add the associated box to the pixa.
         * We also keep track of the centroid of each pix,
         * and use the difference between centroids (of the
         * pix with the exemplar we are checking it with)
         * to align the two when checking that the correlation
         * score exceeds a threshold.  The correlation score
         * is given by the square of the area of the AND
         * between aligned instance and template, divided by
         * the product of areas of each image.  For identical
         * template and instance, the score is 1.0.
         * If the threshold is too small, non-equivalent instances
         * will be placed in the same class; if too large, there will
         * be an unnecessary division of classes representing the
         * same character.  The weightfactor adds in some of the
         * difference (1.0 - thresh), depending on the heaviness
         * of the template (measured as the fraction of fg pixels). */
    thresh = classer->thresh;
    weight = classer->weightfactor;
    naarea = classer->naarea;
    dahash = classer->dahash;
    for (i = 0; i < n; i++) {
        pix1 = pixaGetPix(pixa1, i, L_CLONE);
        area1 = pixcts[i];
        ptaGetPt(pta, i, &x1, &y1);  /* centroid for this instance */
        nt = pixaGetCount(pixat);
        found = FALSE;
        findcontext = findSimilarSizedTemplatesInit(classer, pix1);
        while ( (iclass = findSimilarSizedTemplatesNext(findcontext)) > -1) {
                /* Get the template */
            pix2 = pixaGetPix(pixat, iclass, L_CLONE);
            numaGetIValue(nafgt, iclass, &area2);
            ptaGetPt(ptact, iclass, &x2, &y2);  /* template centroid */

                /* Find threshold for this template */
            if (weight > 0.0) {
                numaGetIValue(naarea, iclass, &area);
                threshold = thresh + (1.f - thresh) * weight * area2 / area;
            } else {
                threshold = thresh;
            }

                /* Find score for this template */
            overthreshold = pixCorrelationScoreThresholded(pix1, pix2,
                                         area1, area2, x1 - x2, y1 - y2,
                                         MAX_DIFF_WIDTH, MAX_DIFF_HEIGHT,
                                         sumtab, pixrowcts[i], threshold);
#if DEBUG_CORRELATION_SCORE
            {
                l_float32 score, testscore;
                l_int32 count, testcount;
                pixCorrelationScore(pix1, pix2, area1, area2, x1 - x2, y1 - y2,
                                    MAX_DIFF_WIDTH, MAX_DIFF_HEIGHT,
                                    sumtab, &score);

                pixCorrelationScoreSimple(pix1, pix2, area1, area2,
                                          x1 - x2, y1 - y2, MAX_DIFF_WIDTH,
                                          MAX_DIFF_HEIGHT, sumtab, &testscore);
                count = (l_int32)rint(sqrt(score * area1 * area2));
                testcount = (l_int32)rint(sqrt(testscore * area1 * area2));
                if ((score >= threshold) != (testscore >= threshold)) {
                    lept_stderr("Correlation score mismatch: "
                                "%d(%g,%d) vs %d(%g,%d) (%g)\n",
                                count, score, score >= threshold,
                                testcount, testscore, testscore >= threshold,
                                score - testscore);
                }

                if ((score >= threshold) != overthreshold) {
                    lept_stderr("Mismatch between correlation/threshold "
                                "comparison: %g(%g,%d) >= %g(%g) vs %s\n",
                                score, score*area1*area2, count, threshold,
                                threshold*area1*area2,
                                (overthreshold ? "true" : "false"));
                }
            }
#endif  /* DEBUG_CORRELATION_SCORE */
            pixDestroy(&pix2);

            if (overthreshold) {  /* greedy match */
                found = TRUE;
                numaAddNumber(naclass, (l_float32)iclass);
                numaAddNumber(napage, (l_float32)npages);
                if (classer->keep_pixaa) {
                        /* We are keeping a record of all components */
                    pixa = pixaaGetPixa(pixaa, iclass, L_CLONE);
                    pix = pixaGetPix(pixas, i, L_CLONE);
                    pixaAddPix(pixa, pix, L_INSERT);
                    box = boxaGetBox(boxa, i, L_CLONE);
                    pixaAddBox(pixa, box, L_INSERT);
                    pixaDestroy(&pixa);
                }
                break;
            }
        }
        findSimilarSizedTemplatesDestroy(&findcontext);
        if (found == FALSE) {  /* new class */
            numaAddNumber(naclass, (l_float32)nt);
            numaAddNumber(napage, (l_float32)npages);
            pixa = pixaCreate(0);
            pix = pixaGetPix(pixas, i, L_CLONE);  /* unbordered instance */
            pixaAddPix(pixa, pix, L_INSERT);
            wt = pixGetWidth(pix);
            ht = pixGetHeight(pix);
            l_dnaHashAdd(dahash, (l_uint64)ht * wt, nt);
            box = boxaGetBox(boxa, i, L_CLONE);
            pixaAddBox(pixa, box, L_INSERT);
            pixaaAddPixa(pixaa, pixa, L_INSERT);  /* unbordered instance */
            ptaAddPt(ptact, x1, y1);
            numaAddNumber(nafgt, (l_float32)area1);
            pixaAddPix(pixat, pix1, L_INSERT);   /* bordered template */
            area = (pixGetWidth(pix1) - 2 * JB_ADDED_PIXELS) *
                   (pixGetHeight(pix1) - 2 * JB_ADDED_PIXELS);
            numaAddNumber(naarea, (l_float32)area);
        } else {  /* don't save it */
            pixDestroy(&pix1);
        }
    }
    classer->nclass = pixaGetCount(pixat);

    LEPT_FREE(pixcts);
    LEPT_FREE(centtab);
    for (i = 0; i < n; i++) {
        LEPT_FREE(pixrowcts[i]);
    }
    LEPT_FREE(pixrowcts);

    LEPT_FREE(sumtab);
    ptaDestroy(&pta);
    pixaDestroy(&pixa1);
    return 0;
}


/*----------------------------------------------------------------------*
 *             Determine the image components we start with             *
 *----------------------------------------------------------------------*/
/*!
 * \brief   jbGetComponents()
 *
 * \param[in]    pixs        1 bpp
 * \param[in]    components  JB_CONN_COMPS, JB_CHARACTERS, JB_WORDS
 * \param[in]    maxwidth    of saved components; larger are discarded
 * \param[in]    maxheight   of saved components; larger are discarded
 * \param[out]   ppboxa      b.b. of component items
 * \param[out]   pppixa      component items
 * \return  0 if OK, 1 on error
 */
l_ok
jbGetComponents(PIX     *pixs,
                l_int32  components,
                l_int32  maxwidth,
                l_int32  maxheight,
                BOXA   **pboxad,
                PIXA   **ppixad)
{
l_int32    empty, res, redfactor;
BOXA      *boxa;
PIX       *pix1, *pix2, *pix3;
PIXA      *pixa, *pixat;

    if (!pboxad)
        return ERROR_INT("&boxad not defined", __func__, 1);
    *pboxad = NULL;
    if (!ppixad)
        return ERROR_INT("&pixad not defined", __func__, 1);
    *ppixad = NULL;
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (components != JB_CONN_COMPS && components != JB_CHARACTERS &&
        components != JB_WORDS)
        return ERROR_INT("invalid components", __func__, 1);

    pixZero(pixs, &empty);
    if (empty) {
        *pboxad = boxaCreate(0);
        *ppixad = pixaCreate(0);
        return 0;
    }

        /* If required, preprocess input pixs.  The method for both
         * characters and words is to generate a connected component
         * mask over the units that we want to aggregrate, which are,
         * in general, sets of related connected components in pixs.
         * For characters, we want to include the dots with
         * 'i', 'j' and '!', so we do a small vertical closing to
         * generate the mask.  For words, we make a mask over all
         * characters in each word.  This is a bit more tricky, because
         * the spacing between words is difficult to predict a priori,
         * and words can be typeset with variable spacing that can
         * in some cases be barely larger than the space between
         * characters.  The first step is to generate the mask and
         * identify each of its connected components.  */
    if (components == JB_CONN_COMPS) {  /* no preprocessing */
        boxa = pixConnComp(pixs, &pixa, 8);
    } else if (components == JB_CHARACTERS) {
        pix1 = pixMorphSequence(pixs, "c1.6", 0);
        boxa = pixConnComp(pix1, &pixat, 8);
        pixa = pixaClipToPix(pixat, pixs);
        pixDestroy(&pix1);
        pixaDestroy(&pixat);
    } else {  /* components == JB_WORDS */

            /* Do the operations at about 150 ppi resolution.
             * It is much faster at 75 ppi, but the results are
             * more accurate at 150 ppi.  This will segment the
             * words in body text.  It can be expected that relatively
             * infrequent words in a larger font will be split. */
        res = pixGetXRes(pixs);
        if (res <= 200) {
            redfactor = 1;
            pix1 = pixClone(pixs);
        } else if (res <= 400) {
            redfactor = 2;
            pix1 = pixReduceRankBinaryCascade(pixs, 1, 0, 0, 0);
        } else {
            redfactor = 4;
            pix1 = pixReduceRankBinaryCascade(pixs, 1, 1, 0, 0);
        }

            /* Estimate the word mask, at approximately 150 ppi.
             * This has both very large and very small components left in. */
        pixWordMaskByDilation(pix1, &pix2, NULL, NULL);

            /* Expand the optimally dilated word mask to full res. */
        pix3 = pixExpandReplicate(pix2, redfactor);

            /* Pull out the pixels in pixs corresponding to the mask
             * components in pix3.  Note that above we used threshold
             * levels in the reduction of 1 to insure that the resulting
             * mask fully covers the input pixs.  The downside of using
             * a threshold of 1 is that very close characters from adjacent
             * lines can be joined.  But with a level of 2 or greater,
             * it is necessary to use a seedfill, followed by a pixOr():
             *       pixt4 = pixSeedfillBinary(NULL, pix3, pixs, 8);
             *       pixOr(pix3, pix3, pixt4);
             * to insure that the mask coverage is complete over pixs.  */
        boxa = pixConnComp(pix3, &pixat, 4);
        pixa = pixaClipToPix(pixat, pixs);
        pixaDestroy(&pixat);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
        pixDestroy(&pix3);
    }

        /* Remove large components, and save the results.  */
    *ppixad = pixaSelectBySize(pixa, maxwidth, maxheight, L_SELECT_IF_BOTH,
                               L_SELECT_IF_LTE, NULL);
    *pboxad = boxaSelectBySize(boxa, maxwidth, maxheight, L_SELECT_IF_BOTH,
                               L_SELECT_IF_LTE, NULL);
    pixaDestroy(&pixa);
    boxaDestroy(&boxa);

    return 0;
}


/*!
 * \brief   pixWordMaskByDilation()
 *
 * \param[in]    pixs    1 bpp; typ. at 75 to 150 ppi
 * \param[out]   pmask   [optional] dilated word mask
 * \param[out]   psize   [optional] size of good horizontal dilation
 * \param[out]   pixadb  [optional] debug: pixa of intermediate steps
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This gives an estimate of the word masks.  See
 *          pixWordBoxesByDilation() for further filtering of the word boxes.
 *      (2) The resolution should be between 75 and 150 ppi, and the optimal
 *          dilation will be between 3 and 10.
 *      (3) A good size for dilating to get word masks is optionally returned.
 *      (4) Typically, the number of c.c. reduced with each successive
 *          dilation (stored in nadiff) decreases quickly to a minimum
 *          (where the characters in a word are joined), and then
 *          increases again as the smaller number of words are joined.
 *          For the typical case, you can then look for this minimum
 *          and dilate to get the word mask.  However, there are many
 *          cases where the function is not so simple. For example, if the
 *          pix has been upscaled 2x, the nadiff function oscillates, with
 *          every other value being zero!  And for some images it tails
 *          off without a clear minimum to indicate where to break.
 *          So a more simple and robust method is to find the dilation
 *          where the initial number of c.c. has been reduced by some
 *          fraction (we use a 70% reduction).
 * </pre>
 */
l_ok
pixWordMaskByDilation(PIX      *pixs,
                      PIX     **ppixm,
                      l_int32  *psize,
                      PIXA     *pixadb)
{
l_int32   i, n, ndil, maxdiff, diff, ibest;
l_int32   check, count, total, xres;
l_int32   ncc[13];  /* max dilation + 1 */
l_int32  *diffa;
BOXA     *boxa;
NUMA     *nacc, *nadiff;
PIX      *pix1, *pix2;

    if (ppixm) *ppixm = NULL;
    if (psize) *psize = 0;
    if (!pixs || pixGetDepth(pixs) != 1)
        return ERROR_INT("pixs undefined or not 1 bpp", __func__, 1);
    if (!ppixm && !psize)
        return ERROR_INT("no output requested", __func__, 1);

        /* Find a good dilation to create the word mask, by successively
         * increasing dilation size and counting the connected components. */
    pix1 = pixCopy(NULL, pixs);
    ndil = 12;  /* appropriate for 75 to 150 ppi */
    nacc = numaCreate(ndil + 1);
    nadiff = numaCreate(ndil + 1);
    for (i = 0; i <= ndil; i++) {
        if (i == 0)  /* first one not dilated */
            pix2 = pixCopy(NULL, pix1);
        else  /* successive dilation by sel_2h */
            pix2 = pixMorphSequence(pix1, "d2.1", 0);
        boxa = pixConnCompBB(pix2, 4);
        ncc[i] = boxaGetCount(boxa);
        numaAddNumber(nacc, (l_float32)ncc[i]);
        if (i == 0) total = ncc[0];
        if (i > 0) {
            diff = ncc[i - 1] - ncc[i];
            numaAddNumber(nadiff, (l_float32)diff);
        }
        pixDestroy(&pix1);
        pix1 = pix2;
        boxaDestroy(&boxa);
    }
    pixDestroy(&pix1);

        /* Find the dilation at which the c.c. count has reduced
         * to 30% of the initial value.  Although 30% seems high,
         * it seems better to use this but add one to ibest.  */
    diffa = numaGetIArray(nadiff);
    n = numaGetCount(nadiff);
    maxdiff = 0;
    check = TRUE;
    ibest = 2;
    for (i = 1; i < n; i++) {
        numaGetIValue(nacc, i, &count);
        if (check && count < 0.3 * total) {
            ibest = i + 1;
            check = FALSE;
        }
        diff = diffa[i];
        if (diff > maxdiff)
            maxdiff = diff;
    }
    LEPT_FREE(diffa);

        /* Add small compensation for higher resolution */
    xres = pixGetXRes(pixs);
    if (xres == 0) xres = 150;
    if (xres > 110) ibest++;
    if (ibest < 2) {
        L_INFO("setting ibest to minimum allowed value of 2\n", __func__);
        ibest = 2;
    }

    if (pixadb) {
        lept_mkdir("lept/jb");
        {NUMA  *naseq;
         PIX   *pix3, *pix4;
            L_INFO("Best dilation: %d\n", __func__, L_MAX(3, ibest + 1));
            naseq = numaMakeSequence(1, 1, numaGetCount(nacc));
            pix3 = gplotGeneralPix2(naseq, nacc, GPLOT_LINES,
                                    "/tmp/lept/jb/numcc",
                                    "Number of cc vs. horizontal dilation",
                                    "Sel horiz", "Number of cc");
            pixaAddPix(pixadb, pix3, L_INSERT);
            numaDestroy(&naseq);
            naseq = numaMakeSequence(1, 1, numaGetCount(nadiff));
            pix3 = gplotGeneralPix2(naseq, nadiff, GPLOT_LINES,
                                    "/tmp/lept/jb/diffcc",
                                    "Diff count of cc vs. horizontal dilation",
                                    "Sel horiz", "Diff in cc");
            pixaAddPix(pixadb, pix3, L_INSERT);
            numaDestroy(&naseq);
            pix3 = pixCloseBrick(NULL, pixs, ibest + 1, 1);
            pix4 = pixScaleToSize(pix3, 600, 0);
            pixaAddPix(pixadb, pix4, L_INSERT);
            pixDestroy(&pix3);
        }
    }

    if (psize) *psize = ibest + 1;
    if (ppixm)
        *ppixm = pixCloseBrick(NULL, pixs, ibest + 1, 1);

    numaDestroy(&nacc);
    numaDestroy(&nadiff);
    return 0;
}


/*!
 * \brief   pixWordBoxesByDilation()
 *
 * \param[in]    pixs       1 bpp; typ. 75 - 200 ppi
 * \param[in]    minwidth   saved components; smaller are discarded
 * \param[in]    minheight  saved components; smaller are discarded
 * \param[in]    maxwidth   saved components; larger are discarded
 * \param[in]    maxheight  saved components; larger are discarded
 * \param[out]   pboxa      of dilated word mask
 * \param[out]   psize      [optional] size of good horizontal dilation
 * \param[out]   pixadb     [optional] debug: pixa of intermediate steps
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Returns a pruned set of word boxes.
 *      (2) See pixWordMaskByDilation().
 * </pre>
 */
l_ok
pixWordBoxesByDilation(PIX      *pixs,
                       l_int32   minwidth,
                       l_int32   minheight,
                       l_int32   maxwidth,
                       l_int32   maxheight,
                       BOXA    **pboxa,
                       l_int32  *psize,
                       PIXA     *pixadb)
{
BOXA  *boxa1, *boxa2;
PIX   *pix1, *pix2;

    if (psize) *psize = 0;
    if (!pixs || pixGetDepth(pixs) != 1)
        return ERROR_INT("pixs undefined or not 1 bpp", __func__, 1);
    if (!pboxa)
        return ERROR_INT("&boxa not defined", __func__, 1);
    *pboxa = NULL;

        /* Make a first estimate of the word mask */
    if (pixWordMaskByDilation(pixs, &pix1, psize, pixadb))
        return ERROR_INT("pixWordMaskByDilation() failed", __func__, 1);

        /* Prune the word mask.  Get the bounding boxes of the words.
         * Remove the small ones, which can be due to punctuation
         * that was not joined to a word.  Also remove the large ones,
         * which are not likely to be words. */
    boxa1 = pixConnComp(pix1, NULL, 8);
    boxa2 = boxaSelectBySize(boxa1, minwidth, minheight, L_SELECT_IF_BOTH,
                             L_SELECT_IF_GTE, NULL);
    *pboxa = boxaSelectBySize(boxa2, maxwidth, maxheight, L_SELECT_IF_BOTH,
                             L_SELECT_IF_LTE, NULL);
    if (pixadb) {
        pix2 = pixUnpackBinary(pixs, 32, 1);
        pixRenderBoxaArb(pix2, boxa1, 2, 255, 0, 0);
        pixaAddPix(pixadb, pix2, L_INSERT);
        pix2 = pixUnpackBinary(pixs, 32, 1);
        pixRenderBoxaArb(pix2, boxa2, 2, 0, 255, 0);
        pixaAddPix(pixadb, pix2, L_INSERT);
    }
    boxaDestroy(&boxa1);
    boxaDestroy(&boxa2);
    pixDestroy(&pix1);
    return 0;
}


/*----------------------------------------------------------------------*
 *                 Build grayscale composites (templates)               *
 *----------------------------------------------------------------------*/
/*!
 * \brief   jbAccumulateComposites()
 *
 * \param[in]    pixaa   one pixa for each class
 * \param[out]   ppna    number of samples used to build each composite
 * \param[out]   pptat   centroids of bordered composites
 * \return  pixad accumulated sum of samples in each class, or NULL on error
 *
 */
PIXA *
jbAccumulateComposites(PIXAA  *pixaa,
                       NUMA  **pna,
                       PTA   **pptat)
{
l_int32    n, nt, i, j, d, minw, maxw, minh, maxh, xdiff, ydiff;
l_float32  x, y, xave, yave;
NUMA      *na;
PIX       *pix, *pixt1, *pixt2, *pixsum;
PIXA      *pixa, *pixad;
PTA       *ptat, *pta;

    if (!pptat)
        return (PIXA *)ERROR_PTR("&ptat not defined", __func__, NULL);
    *pptat = NULL;
    if (!pna)
        return (PIXA *)ERROR_PTR("&na not defined", __func__, NULL);
    *pna = NULL;
    if (!pixaa)
        return (PIXA *)ERROR_PTR("pixaa not defined", __func__, NULL);

    n = pixaaGetCount(pixaa, NULL);
    if ((ptat = ptaCreate(n)) == NULL)
        return (PIXA *)ERROR_PTR("ptat not made", __func__, NULL);
    *pptat = ptat;
    pixad = pixaCreate(n);
    na = numaCreate(n);
    *pna = na;

    for (i = 0; i < n; i++) {
        pixa = pixaaGetPixa(pixaa, i, L_CLONE);
        nt = pixaGetCount(pixa);
        numaAddNumber(na, nt);
        if (nt == 0) {
            L_WARNING("empty pixa found!\n", __func__);
            pixaDestroy(&pixa);
            continue;
        }
        pixaSizeRange(pixa, &minw, &minh, &maxw, &maxh);
        pix = pixaGetPix(pixa, 0, L_CLONE);
        d = pixGetDepth(pix);
        pixDestroy(&pix);
        pixt1 = pixCreate(maxw, maxh, d);
        pixsum = pixInitAccumulate(maxw, maxh, 0);
        pta = pixaCentroids(pixa);

            /* Find the average value of the centroids ... */
        xave = yave = 0;
        for (j = 0; j < nt; j++) {
            ptaGetPt(pta, j, &x, &y);
            xave += x;
            yave += y;
        }
        xave = xave / (l_float32)nt;
        yave = yave / (l_float32)nt;

            /* and place all centroids at their average value */
        for (j = 0; j < nt; j++) {
            pixt2 = pixaGetPix(pixa, j, L_CLONE);
            ptaGetPt(pta, j, &x, &y);
            xdiff = (l_int32)(x - xave);
            ydiff = (l_int32)(y - yave);
            pixClearAll(pixt1);
            pixRasterop(pixt1, xdiff, ydiff, maxw, maxh, PIX_SRC,
                        pixt2, 0, 0);
            pixAccumulate(pixsum, pixt1, L_ARITH_ADD);
            pixDestroy(&pixt2);
        }
        pixaAddPix(pixad, pixsum, L_INSERT);
        ptaAddPt(ptat, xave, yave);

        pixaDestroy(&pixa);
        pixDestroy(&pixt1);
        ptaDestroy(&pta);
    }

    return pixad;
}


/*!
 * \brief   jbTemplatesFromComposites()
 *
 * \param[in]    pixac   one pix of composites for each class
 * \param[in]    na      number of samples used for each class composite
 * \return  pixad 8 bpp templates for each class, or NULL on error
 *
 */
PIXA *
jbTemplatesFromComposites(PIXA  *pixac,
                          NUMA  *na)
{
l_int32    n, i;
l_float32  nt;  /* number of samples in the composite; always an integer */
l_float32  factor;
PIX       *pixsum;   /* accumulated composite */
PIX       *pixd;
PIXA      *pixad;

    if (!pixac)
        return (PIXA *)ERROR_PTR("pixac not defined", __func__, NULL);
    if (!na)
        return (PIXA *)ERROR_PTR("na not defined", __func__, NULL);

    n = pixaGetCount(pixac);
    pixad = pixaCreate(n);
    for (i = 0; i < n; i++) {
        pixsum = pixaGetPix(pixac, i, L_COPY);  /* changed internally */
        numaGetFValue(na, i, &nt);
        factor = 255.f / nt;
        pixMultConstAccumulate(pixsum, factor, 0);  /* changes pixsum */
        pixd = pixFinalAccumulate(pixsum, 0, 8);
        pixaAddPix(pixad, pixd, L_INSERT);
        pixDestroy(&pixsum);
    }

    return pixad;
}



/*----------------------------------------------------------------------*
 *                       jbig2 utility routines                         *
 *----------------------------------------------------------------------*/
/*!
 * \brief   jbClasserCreate()
 *
 * \param[in]    method      JB_RANKHAUS, JB_CORRELATION
 * \param[in]    components  JB_CONN_COMPS, JB_CHARACTERS, JB_WORDS
 * \return  jbclasser, or NULL on error
 */
JBCLASSER *
jbClasserCreate(l_int32  method,
                l_int32  components)
{
JBCLASSER  *classer;

    if (method != JB_RANKHAUS && method != JB_CORRELATION)
        return (JBCLASSER *)ERROR_PTR("invalid method", __func__, NULL);
    if (components != JB_CONN_COMPS && components != JB_CHARACTERS &&
        components != JB_WORDS)
        return (JBCLASSER *)ERROR_PTR("invalid component", __func__, NULL);

    classer = (JBCLASSER *)LEPT_CALLOC(1, sizeof(JBCLASSER));
    classer->method = method;
    classer->components = components;
    classer->nacomps = numaCreate(0);
    classer->pixaa = pixaaCreate(0);
    classer->pixat = pixaCreate(0);
    classer->pixatd = pixaCreate(0);
    classer->nafgt = numaCreate(0);
    classer->naarea = numaCreate(0);
    classer->ptac = ptaCreate(0);
    classer->ptact = ptaCreate(0);
    classer->naclass = numaCreate(0);
    classer->napage = numaCreate(0);
    classer->ptaul = ptaCreate(0);
    return classer;
}


/*
 * \brief   jbClasserDestroy()
 *
 * \param[in,out]  pclasser   will be set to null before returning
 * \return  void
 */
void
jbClasserDestroy(JBCLASSER  **pclasser)
{
JBCLASSER  *classer;

    if (!pclasser)
        return;
    if ((classer = *pclasser) == NULL)
        return;

    sarrayDestroy(&classer->safiles);
    numaDestroy(&classer->nacomps);
    pixaaDestroy(&classer->pixaa);
    pixaDestroy(&classer->pixat);
    pixaDestroy(&classer->pixatd);
    l_dnaHashDestroy(&classer->dahash);
    numaDestroy(&classer->nafgt);
    numaDestroy(&classer->naarea);
    ptaDestroy(&classer->ptac);
    ptaDestroy(&classer->ptact);
    numaDestroy(&classer->naclass);
    numaDestroy(&classer->napage);
    ptaDestroy(&classer->ptaul);
    ptaDestroy(&classer->ptall);
    LEPT_FREE(classer);
    *pclasser = NULL;
}


/*!
 * \brief   jbDataSave()
 *
 * \param[in]    jbclasser
 * \param[in]    latticew   cell width used to store each connected
 *                          component in the composite
 * \param[in]    latticeh   ditto for cell height
 * \return  jbdata, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This routine stores the jbig2-type data required for
 *          generating a lossy jbig2 version of the image.
 *          It can be losslessly written to (and read from) two files.
 *      (2) It generates and stores the mosaic of templates.
 *      (3) It clones the Numa and Pta arrays, so these must all
 *          be destroyed by the caller.
 *      (4) Input 0 to use the default values for latticew and/or latticeh,
 * </pre>
 */
JBDATA *
jbDataSave(JBCLASSER  *classer)
{
l_int32  maxw, maxh;
JBDATA  *data;
PIX     *pix;

    if (!classer)
        return (JBDATA *)ERROR_PTR("classer not defined", __func__, NULL);

        /* Write the templates into an array. */
    pixaSizeRange(classer->pixat, NULL, NULL, &maxw, &maxh);
    pix = pixaDisplayOnLattice(classer->pixat, maxw + 1, maxh + 1,
                               NULL, NULL);
    if (!pix)
        return (JBDATA *)ERROR_PTR("data not made", __func__, NULL);

    data = (JBDATA *)LEPT_CALLOC(1, sizeof(JBDATA));
    data->pix = pix;
    data->npages = classer->npages;
    data->w = classer->w;
    data->h = classer->h;
    data->nclass = classer->nclass;
    data->latticew = maxw + 1;
    data->latticeh = maxh + 1;
    data->naclass = numaClone(classer->naclass);
    data->napage = numaClone(classer->napage);
    data->ptaul = ptaClone(classer->ptaul);
    return data;
}


/*
 * \brief   jbDataDestroy()
 *
 * \param[in,out]  pdata   will be set to null before returning
 * \return  void
 */
void
jbDataDestroy(JBDATA  **pdata)
{
JBDATA  *data;

    if (!pdata)
        return;
    if ((data = *pdata) == NULL)
        return;

    pixDestroy(&data->pix);
    numaDestroy(&data->naclass);
    numaDestroy(&data->napage);
    ptaDestroy(&data->ptaul);
    LEPT_FREE(data);
    *pdata = NULL;
}


/*!
 * \brief   jbDataWrite()
 *
 * \param[in]  rootname    for output files; everything but the extension
 * \param[in]  jbdata
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Serialization function that writes data in jbdata to file.
 * </pre>
 */
l_ok
jbDataWrite(const char  *rootout,
            JBDATA      *jbdata)
{
char     buf[L_BUF_SIZE];
l_int32  w, h, nclass, npages, cellw, cellh, ncomp, i, x, y, iclass, ipage;
NUMA    *naclass, *napage;
PTA     *ptaul;
PIX     *pixt;
FILE    *fp;

    if (!rootout)
        return ERROR_INT("no rootout", __func__, 1);
    if (!jbdata)
        return ERROR_INT("no jbdata", __func__, 1);

    npages = jbdata->npages;
    w = jbdata->w;
    h = jbdata->h;
    pixt = jbdata->pix;
    nclass = jbdata->nclass;
    cellw = jbdata->latticew;
    cellh = jbdata->latticeh;
    naclass = jbdata->naclass;
    napage = jbdata->napage;
    ptaul = jbdata->ptaul;

    snprintf(buf, L_BUF_SIZE, "%s%s", rootout, JB_TEMPLATE_EXT);
    pixWrite(buf, pixt, IFF_PNG);

    snprintf(buf, L_BUF_SIZE, "%s%s", rootout, JB_DATA_EXT);
    if ((fp = fopenWriteStream(buf, "wb")) == NULL)
        return ERROR_INT_1("stream not opened", buf, __func__, 1);
    ncomp = ptaGetCount(ptaul);
    fprintf(fp, "jb data file\n");
    fprintf(fp, "num pages = %d\n", npages);
    fprintf(fp, "page size: w = %d, h = %d\n", w, h);
    fprintf(fp, "num components = %d\n", ncomp);
    fprintf(fp, "num classes = %d\n", nclass);
    fprintf(fp, "template lattice size: w = %d, h = %d\n", cellw, cellh);
    for (i = 0; i < ncomp; i++) {
        numaGetIValue(napage, i, &ipage);
        numaGetIValue(naclass, i, &iclass);
        ptaGetIPt(ptaul, i, &x, &y);
        fprintf(fp, "%d %d %d %d\n", ipage, iclass, x, y);
    }
    fclose(fp);

    return 0;
}


/*!
 * \brief   jbDataRead()
 *
 * \param[in]  rootname    for template and data files
 * \return  jbdata, or NULL on error
 */
JBDATA *
jbDataRead(const char  *rootname)
{
char      fname[L_BUF_SIZE];
char     *linestr;
l_uint8  *data;
l_int32   nsa, i, w, h, cellw, cellh, x, y, iclass, ipage;
l_int32   npages, nclass, ncomp, ninit;
size_t    size;
JBDATA   *jbdata;
NUMA     *naclass, *napage;
PIX      *pixs;
PTA      *ptaul;
SARRAY   *sa;

    if (!rootname)
        return (JBDATA *)ERROR_PTR("rootname not defined", __func__, NULL);

    snprintf(fname, L_BUF_SIZE, "%s%s", rootname, JB_TEMPLATE_EXT);
    if ((pixs = pixRead(fname)) == NULL)
        return (JBDATA *)ERROR_PTR("pix not read", __func__, NULL);

    snprintf(fname, L_BUF_SIZE, "%s%s", rootname, JB_DATA_EXT);
    if ((data = l_binaryRead(fname, &size)) == NULL) {
        pixDestroy(&pixs);
        return (JBDATA *)ERROR_PTR("data not read", __func__, NULL);
    }

    if ((sa = sarrayCreateLinesFromString((char *)data, 0)) == NULL) {
        pixDestroy(&pixs);
        LEPT_FREE(data);
        return (JBDATA *)ERROR_PTR("sa not made", __func__, NULL);
    }
    nsa = sarrayGetCount(sa);   /* number of cc + 6 */
    linestr = sarrayGetString(sa, 0, L_NOCOPY);
    if (strcmp(linestr, "jb data file") != 0) {
        pixDestroy(&pixs);
        LEPT_FREE(data);
        sarrayDestroy(&sa);
        return (JBDATA *)ERROR_PTR("invalid jb data file", __func__, NULL);
    }
    linestr = sarrayGetString(sa, 1, L_NOCOPY);
    sscanf(linestr, "num pages = %d", &npages);
    linestr = sarrayGetString(sa, 2, L_NOCOPY);
    sscanf(linestr, "page size: w = %d, h = %d", &w, &h);
    linestr = sarrayGetString(sa, 3, L_NOCOPY);
    sscanf(linestr, "num components = %d", &ncomp);
    linestr = sarrayGetString(sa, 4, L_NOCOPY);
    sscanf(linestr, "num classes = %d\n", &nclass);
    linestr = sarrayGetString(sa, 5, L_NOCOPY);
    sscanf(linestr, "template lattice size: w = %d, h = %d\n", &cellw, &cellh);

#if 1
    lept_stderr("num pages = %d\n", npages);
    lept_stderr("page size: w = %d, h = %d\n", w, h);
    lept_stderr("num components = %d\n", ncomp);
    lept_stderr("num classes = %d\n", nclass);
    lept_stderr("template lattice size: w = %d, h = %d\n", cellw, cellh);
#endif

    ninit = ncomp;
    if (ncomp > 1000000) {  /* fuzz protection */
        L_WARNING("ncomp > 1M\n", __func__);
        ninit = 1000000;
    }
    naclass = numaCreate(ninit);
    napage = numaCreate(ninit);
    ptaul = ptaCreate(ninit);
    for (i = 6; i < nsa; i++) {
        linestr = sarrayGetString(sa, i, L_NOCOPY);
        sscanf(linestr, "%d %d %d %d\n", &ipage, &iclass, &x, &y);
        numaAddNumber(napage, (l_float32)ipage);
        numaAddNumber(naclass, (l_float32)iclass);
        ptaAddPt(ptaul, (l_float32)x, (l_float32)y);
    }

    jbdata = (JBDATA *)LEPT_CALLOC(1, sizeof(JBDATA));
    jbdata->pix = pixs;
    jbdata->npages = npages;
    jbdata->w = w;
    jbdata->h = h;
    jbdata->nclass = nclass;
    jbdata->latticew = cellw;
    jbdata->latticeh = cellh;
    jbdata->naclass = naclass;
    jbdata->napage = napage;
    jbdata->ptaul = ptaul;

    LEPT_FREE(data);
    sarrayDestroy(&sa);
    return jbdata;
}


/*!
 * \brief   jbDataRender()
 *
 * \param[in]  jbdata
 * \param[in]  debugflag   if TRUE, writes into 2 bpp pix and adds
 *                         component outlines in color
 * \return  pixa reconstruction of original images, using templates or
 *              NULL on error
 */
PIXA *
jbDataRender(JBDATA  *data,
             l_int32  debugflag)
{
l_int32   i, w, h, cellw, cellh, x, y, iclass, ipage;
l_int32   npages, nclass, ncomp, wp, hp;
BOX      *box;
BOXA     *boxa;
BOXAA    *baa;
NUMA     *naclass, *napage;
PIX      *pixt, *pix1, *pix2, *pixd;
PIXA     *pixat;   /* pixa of templates */
PIXA     *pixad;   /* pixa of output images */
PIXCMAP  *cmap;
PTA      *ptaul;

    if (!data)
        return (PIXA *)ERROR_PTR("data not defined", __func__, NULL);

    npages = data->npages;
    w = data->w;
    h = data->h;
    pixt = data->pix;
    nclass = data->nclass;
    cellw = data->latticew;
    cellh = data->latticeh;
    naclass = data->naclass;
    napage = data->napage;
    ptaul = data->ptaul;
    ncomp = numaGetCount(naclass);

        /* Reconstruct the original set of images from the templates
         * and the data associated with each component.  First,
         * generate the output pixa as a set of empty pix.  For debug,
         * where the bounding boxes of each component will be displayed
         * in red, use 2 bpp colormapped output pix. */
    if ((pixad = pixaCreate(npages)) == NULL)
        return (PIXA *)ERROR_PTR("pixad not made", __func__, NULL);
    for (i = 0; i < npages; i++) {
        if (debugflag == FALSE) {
            pix1 = pixCreate(w, h, 1);
        } else {
            pix1 = pixCreate(w, h, 2);
            cmap = pixcmapCreate(2);
            pixcmapAddColor(cmap, 255, 255, 255);
            pixcmapAddColor(cmap, 0, 0, 0);
            pixSetColormap(pix1, cmap);
        }
        pixaAddPix(pixad, pix1, L_INSERT);
    }

        /* Put the class templates into a pixa. */
    if ((pixat = pixaCreateFromPix(pixt, nclass, cellw, cellh)) == NULL) {
        pixaDestroy(&pixad);
        return (PIXA *)ERROR_PTR("pixat not made", __func__, NULL);
    }

        /* Place each component in the right location on its page.
         * For debug, first generate the boxa of component bounding
         * boxes for each page, and save the results in a boxaa.
         * Nota bene. In general we cannot use rasterop on colormap
         * indices with operations like PIX_SRC | PIX_DST.  So we must
         * do the rasterop of image components first, and then paint
         * the component bounding boxes later.  We can use rasterop
         * on the 2 bpp pix here because the colormap has only two
         * index values, 0 and 1, * so doing a bit-or between pixels
         * only affects the lower-order bit and does not generate
         * spurious colormap indices. */
    if (debugflag == TRUE) {
        baa = boxaaCreate(npages);
        boxa = boxaCreate(0);
        boxaaInitFull(baa, boxa);
        boxaDestroy(&boxa);
    }
    for (i = 0; i < ncomp; i++) {
        numaGetIValue(napage, i, &ipage);
        numaGetIValue(naclass, i, &iclass);
        pix1 = pixaGetPix(pixat, iclass, L_CLONE);  /* the template */
        wp = pixGetWidth(pix1);
        hp = pixGetHeight(pix1);
        ptaGetIPt(ptaul, i, &x, &y);
        pixd = pixaGetPix(pixad, ipage, L_CLONE);   /* the output page */
        if (debugflag == FALSE) {
            pixRasterop(pixd, x, y, wp, hp, PIX_SRC | PIX_DST, pix1, 0, 0);
        } else {
            pix2 = pixConvert1To2Cmap(pix1);
            pixRasterop(pixd, x, y, wp, hp, PIX_SRC | PIX_DST, pix2, 0, 0);
            boxa = boxaaGetBoxa(baa, ipage, L_CLONE);
            box = boxCreate(x, y, wp, hp);
            boxaAddBox(boxa, box, L_INSERT);
            boxaDestroy(&boxa);  /* clone */
            pixDestroy(&pix2);  /* clone */
        }
        pixDestroy(&pix1);   /* clone */
        pixDestroy(&pixd);  /* clone */
    }

        /* For debug, for each page image, render the box outlines in red.
         * This adds a red colormap entry to each page. */
    if (debugflag == TRUE) {
        for (i = 0; i < npages; i++) {
            pixd = pixaGetPix(pixad, i, L_CLONE);
            boxa = boxaaGetBoxa(baa, i, L_CLONE);
            pixRenderBoxaArb(pixd, boxa, 1, 255, 0, 0);
            pixDestroy(&pixd);   /* clone */
            boxaDestroy(&boxa);  /* clone */
        }
        boxaaDestroy(&baa);
    }

    pixaDestroy(&pixat);
    return pixad;
}


/*!
 * \brief   jbGetULCorners()
 *
 * \param[in]  jbclasser
 * \param[in]  pixs       full res image
 * \param[in]  boxa       of c.c. bounding rectangles for this page
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This computes the ptaul field, which has the global UL corners,
 *          adjusted for each specific component, so that each component
 *          can be replaced by the template for its class and have the
 *          centroid in the template in the same position as the
 *          centroid of the original connected component.  It is important
 *          that this be done properly to avoid a wavy baseline in the
 *          result.
 *      (2) The array fields ptac and ptact give the centroids of
 *          those components relative to the UL corner of each component.
 *          Here, we compute the difference in each component, round to
 *          nearest integer, and correct the box->x and box->y by
 *          the appropriate integral difference.
 *      (3) The templates and stored instances are all bordered.
 * </pre>
 */
l_ok
jbGetULCorners(JBCLASSER  *classer,
               PIX        *pixs,
               BOXA       *boxa)
{
l_int32    i, baseindex, index, n, iclass, idelx, idely, x, y, dx, dy;
l_int32   *sumtab;
l_float32  x1, x2, y1, y2, delx, dely;
BOX       *box;
NUMA      *naclass;
PIX       *pixt;
PTA       *ptac, *ptact, *ptaul;

    if (!classer)
        return ERROR_INT("classer not defined", __func__, 1);
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);

    n = boxaGetCount(boxa);
    ptaul = classer->ptaul;
    naclass = classer->naclass;
    ptac = classer->ptac;
    ptact = classer->ptact;
    baseindex = classer->baseindex;  /* num components before this page */
    sumtab = makePixelSumTab8();
    for (i = 0; i < n; i++) {
        index = baseindex + i;
        ptaGetPt(ptac, index, &x1, &y1);
        numaGetIValue(naclass, index, &iclass);
        ptaGetPt(ptact, iclass, &x2, &y2);
        delx = x2 - x1;
        dely = y2 - y1;
        if (delx >= 0)
            idelx = (l_int32)(delx + 0.5);
        else
            idelx = (l_int32)(delx - 0.5);
        if (dely >= 0)
            idely = (l_int32)(dely + 0.5);
        else
            idely = (l_int32)(dely - 0.5);
        if ((box = boxaGetBox(boxa, i, L_CLONE)) == NULL) {
            LEPT_FREE(sumtab);
            return ERROR_INT("box not found", __func__, 1);
        }
        boxGetGeometry(box, &x, &y, NULL, NULL);

            /* Get final increments dx and dy for best alignment */
        pixt = pixaGetPix(classer->pixat, iclass, L_CLONE);
        finalPositioningForAlignment(pixs, x, y, idelx, idely,
                                     pixt, sumtab, &dx, &dy);
/*        if (i % 20 == 0)
            lept_stderr("dx = %d, dy = %d\n", dx, dy); */
        ptaAddPt(ptaul, (l_float32)(x - idelx + dx), (l_float32)(y - idely + dy));
        boxDestroy(&box);
        pixDestroy(&pixt);
    }

    LEPT_FREE(sumtab);
    return 0;
}


/*!
 * \brief   jbGetLLCorners()
 *
 * \param[in]    jbclasser
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This computes the ptall field, which has the global LL corners,
 *          adjusted for each specific component, so that each component
 *          can be replaced by the template for its class and have the
 *          centroid in the template in the same position as the
 *          centroid of the original connected component. It is important
 *          that this be done properly to avoid a wavy baseline in the result.
 *      (2) It is computed here from the corresponding UL corners, where
 *          the input templates and stored instances are all bordered.
 *          This should be done after all pages have been processed.
 *      (3) For proper substitution, the templates whose LL corners are
 *          placed in these locations must be UN-bordered.
 *          This is available for a realistic jbig2 encoder, which would
 *          (1) encode each template without a border, and (2) encode
 *          the position using the LL corner (rather than the UL
 *          corner) because the difference between y-values
 *          of successive instances is typically close to zero.
 * </pre>
 */
l_ok
jbGetLLCorners(JBCLASSER  *classer)
{
l_int32    i, iclass, n, x1, y1, h;
NUMA      *naclass;
PIX       *pix;
PIXA      *pixat;
PTA       *ptaul, *ptall;

    if (!classer)
        return ERROR_INT("classer not defined", __func__, 1);

    ptaul = classer->ptaul;
    naclass = classer->naclass;
    pixat = classer->pixat;

    ptaDestroy(&classer->ptall);
    n = ptaGetCount(ptaul);
    ptall = ptaCreate(n);
    classer->ptall = ptall;

        /* If the templates were bordered, we would add h - 1 to the UL
         * corner y-value.  However, because the templates to be used
         * here have their borders removed, and the borders are
         * JB_ADDED_PIXELS on each side, we add h - 1 - 2 * JB_ADDED_PIXELS
         * to the UL corner y-value.  */
    for (i = 0; i < n; i++) {
        ptaGetIPt(ptaul, i, &x1, &y1);
        numaGetIValue(naclass, i, &iclass);
        pix = pixaGetPix(pixat, iclass, L_CLONE);
        h = pixGetHeight(pix);
        ptaAddPt(ptall, (l_float32)x1, y1 + h - 1 - 2 * JB_ADDED_PIXELS);
        pixDestroy(&pix);
    }

    return 0;
}


/*----------------------------------------------------------------------*
 *                              Static helpers                          *
 *----------------------------------------------------------------------*/
/* When looking for similar matches we check templates whose size is +/- 2 in
 * each direction. This involves 25 possible sizes. This array contains the
 * offsets for each of those positions in a spiral pattern. There are 25 pairs
 * of numbers in this array: even positions are x values. */
static int two_by_two_walk[50] = {
  0, 0,
  0, 1,
  -1, 0,
  0, -1,
  1, 0,
  -1, 1,
  1, 1,
  -1, -1,
  1, -1,
  0, -2,
  2, 0,
  0, 2,
  -2, 0,
  -1, -2,
  1, -2,
  2, -1,
  2, 1,
  1, 2,
  -1, 2,
  -2, 1,
  -2, -1,
  -2, -2,
  2, -2,
  2, 2,
  -2, 2};


/*!
 * \brief   findSimilarSizedTemplatesInit()
 *
 * \param[in]  classer
 * \param[in]  pixs     instance to be matched
 * \return  Allocated context to be used with findSimilar*
 */
static JBFINDCTX *
findSimilarSizedTemplatesInit(JBCLASSER  *classer,
                              PIX        *pixs)
{
JBFINDCTX  *state;

    state = (JBFINDCTX *)LEPT_CALLOC(1, sizeof(JBFINDCTX));
    state->w = pixGetWidth(pixs) - 2 * JB_ADDED_PIXELS;
    state->h = pixGetHeight(pixs) - 2 * JB_ADDED_PIXELS;
    state->classer = classer;
    return state;
}


static void
findSimilarSizedTemplatesDestroy(JBFINDCTX  **pstate)
{
JBFINDCTX  *state;

    if (pstate == NULL) {
        L_WARNING("ptr address is null\n", __func__);
        return;
    }
    if ((state = *pstate) == NULL)
        return;

    l_dnaDestroy(&state->dna);
    LEPT_FREE(state);
    *pstate = NULL;
    return;
}


/*!
 * \brief   findSimilarSizedTemplatesNext()
 *
 * \param[in]   state   from findSimilarSizedTemplatesInit
 * \return  next template number, or -1 when finished
 *
 *  We have a dna hash table that maps template area to a list of template
 *  numbers with that area.  We wish to find similar sized templates,
 *  so we first look for templates with the same width and height, and
 *  then with width + 1, etc.  This walk is guided by the
 *  two_by_two_walk array, above.
 *
 *  We don't want to have to collect the whole list of templates first,
 *  because we hope to find a well-matching template quickly.  So we
 *  keep the context for this walk in an explictit state structure,
 *  and this function acts like a generator.
 */
static l_int32
findSimilarSizedTemplatesNext(JBFINDCTX  *state)
{
l_int32  desiredh, desiredw, size, templ;
PIX     *pixt;

    while(1) {  /* Continue the walk over step 'i' */
        if (state->i >= 25) {  /* all done; didn't find a good match */
            return -1;
        }

        desiredw = state->w + two_by_two_walk[2 * state->i];
        desiredh = state->h + two_by_two_walk[2 * state->i + 1];
        if (desiredh < 1 || desiredw < 1) {  /* invalid size */
            state->i++;
            continue;
        }

        if (!state->dna) {
                /* We have yet to start walking the array for the step 'i' */
            state->dna = l_dnaHashGetDna(state->classer->dahash,
                              (l_uint64)desiredh * desiredw, L_CLONE);
            if (!state->dna) {  /* nothing there */
                state->i++;
                continue;
            }

            state->n = 0;  /* OK, we got a dna. */
        }

            /* Continue working on this dna */
        size = l_dnaGetCount(state->dna);
        for ( ; state->n < size; ) {
            templ = (l_int32)(state->dna->array[state->n++] + 0.5);
            pixt = pixaGetPix(state->classer->pixat, templ, L_CLONE);
            if (pixGetWidth(pixt) - 2 * JB_ADDED_PIXELS == desiredw &&
                pixGetHeight(pixt) - 2 * JB_ADDED_PIXELS == desiredh) {
                pixDestroy(&pixt);
                return templ;
            }
            pixDestroy(&pixt);
        }

            /* Exhausted the dna (no match found); take another step and
             * try again. */
        state->i++;
        l_dnaDestroy(&state->dna);
        continue;
    }
}


/*!
 * \brief   finalPositioningForAlignment()
 *
 * \param[in]  pixs          input page image
 * \param[in]  x, y          location of UL corner of bb of component in pixs
 * \param[in]  idelx, idely  compensation to match centroids of component
 *                           and template
 * \param[in]  pixt          template, with JB_ADDED_PIXELS of padding
 *                           on all sides
 * \param[in]  sumtab        for summing fg pixels in an image
 * \param[in]  pdx, pdy      return delta on position for best match; each
 *                           one is in the set {-1, 0, 1}
 * \return  0 if OK, 1 on error
 *
 */
static l_int32
finalPositioningForAlignment(PIX      *pixs,
                             l_int32   x,
                             l_int32   y,
                             l_int32   idelx,
                             l_int32   idely,
                             PIX      *pixt,
                             l_int32  *sumtab,
                             l_int32  *pdx,
                             l_int32  *pdy)
{
l_int32  w, h, i, j, minx, miny, count, mincount;
PIX     *pixi;  /* clipped from source pixs */
PIX     *pixr;  /* temporary storage */
BOX     *box;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (!pixt)
        return ERROR_INT("pixt not defined", __func__, 1);
    if (!pdx || !pdy)
        return ERROR_INT("&dx and &dy not both defined", __func__, 1);
    if (!sumtab)
        return ERROR_INT("sumtab not defined", __func__, 1);
    *pdx = *pdy = 0;

        /* Use JB_ADDED_PIXELS pixels padding on each side */
    pixGetDimensions(pixt, &w, &h, NULL);
    box = boxCreate(x - idelx - JB_ADDED_PIXELS,
                    y - idely - JB_ADDED_PIXELS, w, h);
    pixi = pixClipRectangle(pixs, box, NULL);
    boxDestroy(&box);
    if (!pixi)
        return ERROR_INT("pixi not made", __func__, 1);

    pixr = pixCreate(pixGetWidth(pixi), pixGetHeight(pixi), 1);
    mincount = 0x7fffffff;
    for (i = -1; i <= 1; i++) {
        for (j = -1; j <= 1; j++) {
            pixCopy(pixr, pixi);
            pixRasterop(pixr, j, i, w, h, PIX_SRC ^ PIX_DST, pixt, 0, 0);
            pixCountPixels(pixr, &count, sumtab);
            if (count < mincount) {
                minx = j;
                miny = i;
                mincount = count;
            }
        }
    }
    pixDestroy(&pixi);
    pixDestroy(&pixr);

    *pdx = minx;
    *pdy = miny;
    return 0;
}
