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
 * \file  partify.c
 * <pre>
 *
 *     Top level
 *         l_int32          partifyFiles()
 *         l_int32          partifyPixac()
 *
 *     Helpers
 *         static BOXA     *pixLocateStaveSets()
 *         static l_int32   boxaRemoveVGaps()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"

    /* Static helplers */
static BOXA *pixLocateStaveSets(PIX *pixs, l_int32 pageno, PIXA *pixadb);
static l_ok boxaRemoveVGaps(BOXA *boxa);

/*---------------------------------------------------------------------*
 *                              Top level                              *
 *---------------------------------------------------------------------*/
/*!
 * \brief   partifyFiles()
 *
 * \param[in]    dirname    directory of files
 * \param[in]    substr     required filename substring; use NULL for all files
 * \param[in]    nparts     number of parts to generate (counting from top)
 * \param[in]    outroot    root name of output pdf files
 * \param[in]    debugfile  [optional] set to NULL for no debug output
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) All page images are compressed in png format into a pixacomp.
 *      (2) Each page image is deskewed, binarized at 300 ppi,
 *          partified into %nparts, and saved in a set of pixacomps
 *          in tiff-g4 format.
 *      (3) Each partified pixacomp is rendered into a set of page images,
 *          and output as a pdf.
 * </pre>
 */
l_ok
partifyFiles(const char  *dirname,
             const char  *substr,
             l_int32      nparts,
             const char  *outroot,
             const char  *debugfile)
{
PIXA   *pixadb;
PIXAC  *pixac;

    if (!dirname)
        return ERROR_INT("dirname not defined", __func__, 1);
    if (nparts < 0 || nparts > 10)
        return ERROR_INT("nparts not in [1 ... 10]", __func__, 1);
    if (!outroot || outroot[0] == '\n')
        return ERROR_INT("outroot undefined or empty", __func__, 1);

    pixadb = (debugfile) ? pixaCreate(0) : NULL;
    pixac = pixacompCreateFromFiles(dirname, substr, IFF_PNG);
    partifyPixac(pixac, nparts, outroot, pixadb);
    if (pixadb) {
        L_INFO("writing debug output to %s\n", __func__, debugfile);
        pixaConvertToPdf(pixadb, 300, 1.0, L_FLATE_ENCODE, 0,
                         "Partify Debug", debugfile);
    }
    pixacompDestroy(&pixac);
    pixaDestroy(&pixadb);
    return 0;
}


/*!
 * \brief   partifyPixac()
 *
 * \param[in]    pixac      with at least one image
 * \param[in]    nparts     number of parts to generate (counting from top)
 * \param[in]    outroot    root name of output pdf files
 * \param[in]    pixadb     [optional] debug pixa; can be NULL
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) See partifyPixac().
 *      (2) If the image files do not have a resolution, 300 ppi is assumed.
 * </pre>
 */
l_ok
partifyPixac(PIXAC       *pixac,
             l_int32      nparts,
             const char  *outroot,
             PIXA        *pixadb)
{
char       buf[512];
l_int32    i, j, pageno, res, npage, nbox, icount, line;
l_float32  factor;
L_BMF     *bmf;
BOX       *box1, *box2;
BOXA      *boxa1, *boxa2, *boxa3;
PIX       *pix1, *pix2, *pix3, *pix4, *pix5;
PIXAC    **pixaca;

    if (!pixac)
        return ERROR_INT("pixac not defined", __func__, 1);
    if ((npage = pixacompGetCount(pixac)) == 0)
        return ERROR_INT("pixac is empty", __func__, 1);
    if (nparts < 1 || nparts > 10)
        return ERROR_INT("nparts not in [1 ... 10]", __func__, 1);
    if (!outroot || outroot[0] == '\n')
        return ERROR_INT("outroot undefined or empty", __func__, 1);

        /* Initialize the output array for each of the nparts */
    pixaca = (PIXAC **)LEPT_CALLOC(nparts, sizeof(PIXAC *));
    for (i = 0; i < nparts; i++)
        pixaca[i] = pixacompCreate(0);

        /* Process each page */
    line = 1;
    bmf = bmfCreate(NULL, 10);
    for (pageno = 0; pageno < npage; pageno++) {
        if ((pix1 = pixacompGetPix(pixac, pageno)) == NULL) {
            L_ERROR("pix for page %d not found\n", __func__, pageno);
            continue;
        }

            /* Scale, binarize and deskew */
        res = pixGetXRes(pix1);
        if (res == 0 || res == 300 || res > 600) {
            pix2 = pixClone(pix1);
        } else {
            factor = 300.0f / (l_float32)res;
            if (factor > 3)
                L_WARNING("resolution is very low\n", __func__);
            pix2 = pixScale(pix1, factor, factor);
        }
        pix3 = pixConvertTo1Adaptive(pix2);
        pix4 = pixDeskew(pix3, 0);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
        pixDestroy(&pix3);
        if (!pix4) {
            L_ERROR("pix for page %d not deskewed\n", __func__, pageno);
            continue;
        }
        pix1 = pixClone(pix4);  /* rename */
        pixDestroy(&pix4);

            /* Find the stave sets at 4x reduction */
        boxa1 = pixLocateStaveSets(pix1, pageno, pixadb);

            /* Break each stave set into the separate staves (parts).
             * A typical set will have more than one part, but if one of
             * the parts is a keyboard, it will usually have two staves
             * (also called a Grand Staff), composed of treble and
             * bass staves.  For example, a classical violin sonata
             * could have a staff for the violin and two staves for
             * the piano.  We would set nparts == 2, and extract both
             * of the piano staves as the piano part.  */
        nbox = boxaGetCount(boxa1);
        lept_stderr("number of boxes in page %d: %d\n", pageno, nbox);
        for (i = 0; i < nbox; i++, line++) {
            snprintf(buf, sizeof(buf), "%d", line);
            box1 = boxaGetBox(boxa1, i, L_COPY);
            pix2 = pixClipRectangle(pix1, box1, NULL);
            pix3 = pixMorphSequence(pix2, "d1.20 + o50.1 + o1.30", 0);
            boxa2 = pixConnCompBB(pix3, 8);
            boxa3 = boxaSort(boxa2, L_SORT_BY_Y, L_SORT_INCREASING, NULL);
            boxaRemoveVGaps(boxa3);
            icount = boxaGetCount(boxa3);
            if (icount < nparts)
                L_WARNING("nparts requested = %d, but only found %d\n",
                          __func__, nparts, icount);
            for (j = 0; j < icount && j < nparts; j++) {
                box2 = boxaGetBox(boxa3, j, L_COPY);
                if (j == nparts - 1)  /* extend the box to the bottom */
                    boxSetSideLocations(box2, -1, -1, -1,
                                        pixGetHeight(pix1) - 1);
                pix4 = pixClipRectangle(pix2, box2, NULL);
                pix5 = pixAddTextlines(pix4, bmf, buf, 1, L_ADD_LEFT);
                pixacompAddPix(pixaca[j], pix5, IFF_TIFF_G4);
                boxDestroy(&box2);
                pixDestroy(&pix4);
                pixDestroy(&pix5);
            }
            boxaDestroy(&boxa2);
            boxaDestroy(&boxa3);
            boxDestroy(&box1);
            pixDestroy(&pix2);
            pixDestroy(&pix3);
        }
        boxaDestroy(&boxa1);
        pixDestroy(&pix1);
    }

        /* Output separate pdfs for each part */
    for (i = 0; i < nparts; i++) {
        snprintf(buf, sizeof(buf), "%s-%d.pdf", outroot, i);
        L_INFO("writing part %d: %s\n", __func__, i, buf);
        pixacompConvertToPdf(pixaca[i], 300, 1.0, L_G4_ENCODE, 0, NULL, buf);
        pixacompDestroy(&pixaca[i]);
    }
    LEPT_FREE(pixaca);
    bmfDestroy(&bmf);
    return 0;
}


/*
 * \brief   pixLocateStaveSets()
 *
 * \param[in]    pixs       1 bpp, 300 ppi, deskewed
 * \param[in]    pageno     page number; used for debug output
 * \param[in]    pixadb     [optional] debug pixa; can be NULL
 * \return   boxa   containing the stave sets at full resolution
 */
static BOXA *
pixLocateStaveSets(PIX     *pixs,
                   l_int32  pageno,
                   PIXA    *pixadb)
{
BOXA  *boxa1, *boxa2, *boxa3, *boxa4;
PIX   *pix1, *pix2;

    if (!pixs)
        return (BOXA *)ERROR_PTR("pixs not defined", __func__, NULL);

        /* Find the stave sets at 4x reduction */
    pix1 = pixMorphSequence(pixs, "r11", 0);
    boxa1 = pixConnCompBB(pix1, 8);
    boxa2 = boxaSelectByArea(boxa1, 15000, L_SELECT_IF_GT, NULL);
    boxa3 = boxaSort(boxa2, L_SORT_BY_Y, L_SORT_INCREASING, NULL);
    if (pixadb) {
        pix2 = pixConvertTo32(pix1);
        pixRenderBoxaArb(pix2, boxa3, 2, 255, 0, 0);
        pixaAddPix(pixadb, pix2, L_INSERT);
        pixDisplay(pix2, 100 * pageno, 100);
    }
    boxaDestroy(&boxa1);
    boxaDestroy(&boxa2);

    boxaRemoveVGaps(boxa3);
    if (pixadb) {
        pix2 = pixConvertTo32(pix1);
        pixRenderBoxaArb(pix2, boxa3, 2, 0, 255, 0);
        pixaAddPix(pixadb, pix2, L_INSERT);
        pixDisplay(pix2, 100 * pageno, 600);
    }
    boxa4 = boxaTransform(boxa3, 0, 0, 4.0, 4.0);  /* back to full res */
    boxaDestroy(&boxa3);
    pixDestroy(&pix1);
    return boxa4;
}


/*
 * \brief   boxaRemoveVGaps()
 *
 * \param[in]    boxa
 * \return   0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The boxes in %boxa are aligned vertically.  Move the horizontal
 *          edges vertically to remove the gaps between boxes.
 * </pre>
 */
static  l_ok
boxaRemoveVGaps(BOXA  *boxa)
{
l_int32  nbox, i, y1, h1, y2, h2, delta;

    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);
    if ((nbox = boxaGetCount(boxa)) == 0)
        return ERROR_INT("boxa is empty", __func__, 1);
    for (i = 0; i < nbox - 1; i++) {
        boxaGetBoxGeometry(boxa, i, NULL, &y1, NULL, &h1);
        boxaGetBoxGeometry(boxa, i + 1, NULL, &y2, NULL, &h2);
        delta = (y2 - y1 - h1) / 2;
        boxaAdjustBoxSides(boxa, i, 0, 0, 0, delta);
        boxaAdjustBoxSides(boxa, i + 1, 0, 0, -delta, 0);
    }
    boxaAdjustBoxSides(boxa, nbox - 1, 0, 0, 0, delta);  /* bot of last */
    return 0;
}
