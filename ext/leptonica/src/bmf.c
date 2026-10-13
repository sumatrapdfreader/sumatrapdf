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
 * \file bmf.c
 * <pre>
 *
 *   Acquisition and generation of bitmap fonts.
 *
 *       L_BMF           *bmfCreate()
 *       L_BMF           *bmfDestroy()
 *
 *       PIX             *bmfGetPix()
 *       l_int32          bmfGetWidth()
 *       l_int32          bmfGetBaseline()
 *
 *       PIXA            *pixaGetFont()
 *       l_int32          pixaSaveFont()
 *       static PIXA     *pixaGenerateFontFromFile()
 *       static PIXA     *pixaGenerateFontFromString()
 *       static PIXA     *pixaGenerateFont()
 *       static l_int32   pixGetTextBaseline()
 *       static l_int32   bmfMakeAsciiTables()
 *
 *   This is not a very general utility, because it only uses bitmap
 *   representations of a single font, Palatino-Roman, with the
 *   normal style.  It uses bitmaps generated for nine sizes, from
 *   4 to 20 pts, rendered at 300 ppi.  Generalization to different
 *   fonts, styles and sizes is straightforward.
 *
 *   I chose Palatino-Roman is because I like it.
 *   The input font images were generated from a set of small
 *   PostScript files, such as chars-12.ps, which were rendered
 *   into the inputfont[] bitmap files using GhostScript.  See, for
 *   example, the bash script prog/ps2tiff, which will "rip" a
 *   PostScript file into a set of ccitt-g4 compressed tiff files.
 *
 *   The set of ascii characters from 32 through 126 are the 95
 *   printable ascii chars.  Palatino-Roman is missing char 92, '\'.
 *   I have substituted an LR flip of '/', char 47, for 92, so that
 *   there are no missing printable chars in this set.  The space is
 *   char 32, and I have given it a width equal to twice the width of '!'.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"
#include "pix_internal.h"
#include "bmfdata.h"

static const l_float32  VertFractSep = 0.3f;

#ifndef  NO_CONSOLE_IO
#define  DEBUG_BASELINE     0
#define  DEBUG_CHARS        0
#define  DEBUG_FONT_GEN     0
#endif  /* ~NO_CONSOLE_IO */

static PIXA *pixaGenerateFontFromFile(const char *dir, l_int32 fontsize,
                                      l_int32 *pbl0, l_int32 *pbl1,
                                      l_int32 *pbl2);
static PIXA *pixaGenerateFontFromString(l_int32 fontsize, l_int32 *pbl0,
                                        l_int32 *pbl1, l_int32 *pbl2);
static PIXA *pixaGenerateFont(PIX *pixs, l_int32 fontsize, l_int32 *pbl0,
                              l_int32 *pbl1, l_int32 *pbl2);
static l_int32 pixGetTextBaseline(PIX *pixs, l_int32 *tab8, l_int32 *py);
static l_int32 bmfMakeAsciiTables(L_BMF *bmf);

/*---------------------------------------------------------------------*/
/*                           Bmf create/destroy                        */
/*---------------------------------------------------------------------*/
/*!
 * \brief   bmfCreate()
 *
 * \param[in]    dir        [optional] directory holding pixa of character set
 * \param[in]    fontsize   4, 6, 8, ... , 20
 * \return  bmf    holding the bitmap font and associated information
 *
 * <pre>
 * Notes:
 *      (1) If %dir == null, this generates the font bitmaps from a
 *          compiled string.
 *      (2) Otherwise, this tries to read a pre-computed pixa file with the
 *          95 ascii chars in it.  If the file is not found, it then
 *          attempts to generate the pixa and associated baseline
 *          data from a tiff image containing all the characters.  If
 *          that fails, it uses the compiled string.
 * </pre>
 */
L_BMF *
bmfCreate(const char  *dir,
          l_int32      fontsize)
{
L_BMF   *bmf;
PIXA  *pixa;

    if (fontsize < 4 || fontsize > 20 || (fontsize % 2))
        return (L_BMF *)ERROR_PTR("fontsize must be in {4, 6, ..., 20}",
                                  __func__, NULL);

    bmf = (L_BMF *)LEPT_CALLOC(1, sizeof(L_BMF));

    if (!dir) {  /* Generate from a string */
        pixa = pixaGenerateFontFromString(fontsize, &bmf->baseline1,
                                          &bmf->baseline2, &bmf->baseline3);
    } else {  /* Look for the pixa in a directory */
        pixa = pixaGetFont(dir, fontsize, &bmf->baseline1, &bmf->baseline2,
                           &bmf->baseline3);
        if (!pixa) {  /* Not found; make it from a file */
            L_INFO("Generating pixa of bitmap fonts from file\n", __func__);
            pixa = pixaGenerateFontFromFile(dir, fontsize, &bmf->baseline1,
                                            &bmf->baseline2, &bmf->baseline3);
            if (!pixa) {  /* Not made; make it from a string after all */
                L_ERROR("Failed to make font; use string\n", __func__);
                pixa = pixaGenerateFontFromString(fontsize, &bmf->baseline1,
                                          &bmf->baseline2, &bmf->baseline3);
            }
        }
    }

    if (!pixa) {
        bmfDestroy(&bmf);
        return (L_BMF *)ERROR_PTR("font pixa not made", __func__, NULL);
    }

    bmf->pixa = pixa;
    bmf->size = fontsize;
    if (dir) bmf->directory = stringNew(dir);
    bmfMakeAsciiTables(bmf);
    return bmf;
}


/*!
 * \brief   bmfDestroy()
 *
 * \param[in,out]   pbmf    will be set to null before returning
 * \return  void
 */
void
bmfDestroy(L_BMF  **pbmf)
{
L_BMF  *bmf;

    if (pbmf == NULL) {
        L_WARNING("ptr address is null!\n", __func__);
        return;
    }

    if ((bmf = *pbmf) == NULL)
        return;

    pixaDestroy(&bmf->pixa);
    LEPT_FREE(bmf->directory);
    LEPT_FREE(bmf->fonttab);
    LEPT_FREE(bmf->baselinetab);
    LEPT_FREE(bmf);
    *pbmf = NULL;
}


/*---------------------------------------------------------------------*/
/*                             Bmf accessors                           */
/*---------------------------------------------------------------------*/
/*!
 * \brief   bmfGetPix()
 *
 * \param[in]    bmf
 * \param[in]    chr    should be one of the 95 supported printable bitmaps
 * \return  pix    clone of pix in bmf, or NULL on error
 */
PIX *
bmfGetPix(L_BMF  *bmf,
          char    chr)
{
l_int32  i, index;
PIXA    *pixa;

    if ((index = (l_int32)chr) == 10)  /* NL */
        return NULL;
    if (!bmf)
        return (PIX *)ERROR_PTR("bmf not defined", __func__, NULL);
    if (index < 32 || index >= 127)
        return (PIX *)ERROR_PTR("invalid index", __func__, NULL);

    i = bmf->fonttab[index];
    if (i == UNDEF) {
        L_ERROR("no bitmap representation for %d\n", __func__, index);
        return NULL;
    }

    if ((pixa = bmf->pixa) == NULL)
        return (PIX *)ERROR_PTR("pixa not found", __func__, NULL);

    return pixaGetPix(pixa, i, L_CLONE);
}


/*!
 * \brief   bmfGetWidth()
 *
 * \param[in]    bmf
 * \param[in]    chr    should be one of the 95 supported bitmaps
 * \param[out]   pw     character width; -1 if not printable
 * \return  0 if OK, 1 on error
 */
l_ok
bmfGetWidth(L_BMF    *bmf,
            char      chr,
            l_int32  *pw)
{
l_int32  i, index;
PIXA    *pixa;

    if (!pw)
        return ERROR_INT("&w not defined", __func__, 1);
    *pw = -1;
    if (!bmf)
        return ERROR_INT("bmf not defined", __func__, 1);
    if ((index = (l_int32)chr) == 10)  /* NL */
        return 0;
    if (index < 32 || index >= 127)
        return ERROR_INT("invalid index", __func__, 1);

    i = bmf->fonttab[index];
    if (i == UNDEF) {
        L_ERROR("no bitmap representation for %d\n", __func__, index);
        return 1;
    }

    if ((pixa = bmf->pixa) == NULL)
        return ERROR_INT("pixa not found", __func__, 1);

    return pixaGetPixDimensions(pixa, i, pw, NULL, NULL);
}


/*!
 * \brief   bmfGetBaseline()
 *
 * \param[in]    bmf
 * \param[in]    chr         should be one of the 95 supported bitmaps
 * \param[out]   pbaseline   distance below UL corner of bitmap char
 * \return  0 if OK, 1 on error
 */
l_ok
bmfGetBaseline(L_BMF    *bmf,
               char      chr,
               l_int32  *pbaseline)
{
l_int32  bl, index;

    if (!pbaseline)
        return ERROR_INT("&baseline not defined", __func__, 1);
    *pbaseline = 0;
    if (!bmf)
        return ERROR_INT("bmf not defined", __func__, 1);
    if ((index = (l_int32)chr) == 10)  /* NL */
        return 0;
    if (index < 32 || index >= 127)
        return ERROR_INT("invalid index", __func__, 1);

    bl = bmf->baselinetab[index];
    if (bl == UNDEF) {
        L_ERROR("no bitmap representation for %d\n", __func__, index);
        return 1;
    }

    *pbaseline = bl;
    return 0;
}


/*---------------------------------------------------------------------*/
/*               Font bitmap acquisition and generation                */
/*---------------------------------------------------------------------*/
/*!
 * \brief   pixaGetFont()
 *
 * \param[in]    dir         directory holding pixa of character set
 * \param[in]    fontsize    4, 6, 8, ... , 20
 * \param[out]   pbl0        baseline of row 1
 * \param[out]   pbl1        baseline of row 2
 * \param[out]   pbl2        baseline of row 3
 * \return  pixa    of font bitmaps for 95 characters, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This reads a pre-computed pixa file with the 95 ascii chars.
 * </pre>
 */
PIXA *
pixaGetFont(const char  *dir,
            l_int32      fontsize,
            l_int32     *pbl0,
            l_int32     *pbl1,
            l_int32     *pbl2)
{
char     *pathname;
l_int32   fileno;
PIXA     *pixa;

    fileno = (fontsize / 2) - 2;
    if (fileno < 0 || fileno >= NUM_FONTS)
        return (PIXA *)ERROR_PTR("font size invalid", __func__, NULL);
    if (!pbl0 || !pbl1 || !pbl2)
        return (PIXA *)ERROR_PTR("&bl not all defined", __func__, NULL);
    *pbl0 = baselines[fileno][0];
    *pbl1 = baselines[fileno][1];
    *pbl2 = baselines[fileno][2];

    pathname = pathJoin(dir, outputfonts[fileno]);
    pixa = pixaRead(pathname);
    LEPT_FREE(pathname);

    if (!pixa)
        L_WARNING("pixa of char bitmaps not found\n", __func__);
    return pixa;
}


/*!
 * \brief   pixaSaveFont()
 *
 * \param[in]    indir      [optional] directory holding image of character set
 * \param[in]    outdir     directory into which the output pixa file
 *                          will be written
 * \param[in]    fontsize   in pts, at 300 ppi
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This saves a font of a particular size.
 *      (2) If %indir == null, this generates the font bitmaps from a
 *          compiled string.
 *      (3) prog/genfonts calls this function for each of the
 *          nine font sizes, to generate all the font pixa files.
 * </pre>
 */
l_ok
pixaSaveFont(const char  *indir,
             const char  *outdir,
             l_int32      fontsize)
{
char    *pathname;
l_int32  bl1, bl2, bl3;
PIXA    *pixa;

    if (fontsize < 4 || fontsize > 20 || (fontsize % 2))
        return ERROR_INT("fontsize must be in {4, 6, ..., 20}", __func__, 1);

    if (!indir)  /* Generate from a string */
        pixa = pixaGenerateFontFromString(fontsize, &bl1, &bl2, &bl3);
    else  /* Generate from an image file */
        pixa = pixaGenerateFontFromFile(indir, fontsize, &bl1, &bl2, &bl3);
    if (!pixa)
        return ERROR_INT("pixa not made", __func__, 1);

    pathname = pathJoin(outdir, outputfonts[(fontsize - 4) / 2]);
    pixaWrite(pathname, pixa);

#if  DEBUG_FONT_GEN
    L_INFO("Found %d chars in font size %d\n", __func__, pixaGetCount(pixa),
           fontsize);
    L_INFO("Baselines are at: %d, %d, %d\n", __func__, bl1, bl2, bl3);
#endif  /* DEBUG_FONT_GEN */

    LEPT_FREE(pathname);
    pixaDestroy(&pixa);
    return 0;
}


/*!
 * \brief   pixaGenerateFontFromFile()
 *
 * \param[in]    dir        directory holding image of character set
 * \param[in]    fontsize   4, 6, 8, ... , 20, in pts at 300 ppi
 * \param[out]   pbl0       baseline of row 1
 * \param[out]   pbl1       baseline of row 2
 * \param[out]   pbl2       baseline of row 3
 * \return  pixa    of font bitmaps for 95 characters, or NULL on error
 *
 *  These font generation functions use 9 sets, each with bitmaps
 *  of 94 ascii characters, all in Palatino-Roman font.
 *  Each input bitmap has 3 rows of characters.  The range of
 *  ascii values in each row is as follows:
 *    row 0:  32-57   32 is a space
 *    row 1:  58-91   92, '\', is not represented in this font
 *    row 2:  93-126
 *  We LR flip the '/' char to generate a bitmap for the missing
 *  '\' character, so that we have representations of all 95
 *  printable chars.
 *
 *  Typically, use pixaGetFont() to generate the character bitmaps
 *  in memory for a bmf.  This will simply access the bitmap files
 *  in a serialized pixa that were produced in prog/genfonts.c using
 *  this function.
 */
static PIXA *
pixaGenerateFontFromFile(const char  *dir,
                         l_int32      fontsize,
                         l_int32     *pbl0,
                         l_int32     *pbl1,
                         l_int32     *pbl2)
{
char    *pathname;
l_int32  fileno;
PIX     *pix;
PIXA    *pixa;

    if (!pbl0 || !pbl1 || !pbl2)
        return (PIXA *)ERROR_PTR("&bl not all defined", __func__, NULL);
    *pbl0 = *pbl1 = *pbl2 = 0;
    if (!dir)
        return (PIXA *)ERROR_PTR("dir not defined", __func__, NULL);
    fileno = (fontsize / 2) - 2;
    if (fileno < 0 || fileno >= NUM_FONTS)
        return (PIXA *)ERROR_PTR("font size invalid", __func__, NULL);

    pathname = pathJoin(dir, inputfonts[fileno]);
    pix = pixRead(pathname);
    LEPT_FREE(pathname);
    if (!pix) {
        L_ERROR("pix not found for font size %d\n", __func__, fontsize);
        return NULL;
    }

    pixa = pixaGenerateFont(pix, fontsize, pbl0, pbl1, pbl2);
    pixDestroy(&pix);
    return pixa;
}


/*!
 * \brief   pixaGenerateFontFromString()
 *
 * \param[in]    fontsize   4, 6, 8, ... , 20, in pts at 300 ppi
 * \param[out]   pbl0       baseline of row 1
 * \param[out]   pbl1       baseline of row 2
 * \param[out]   pbl2       baseline of row 3
 * \return  pixa   of font bitmaps for 95 characters, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) See pixaGenerateFontFromFile() for details.
 * </pre>
 */
static PIXA *
pixaGenerateFontFromString(l_int32   fontsize,
                           l_int32  *pbl0,
                           l_int32  *pbl1,
                           l_int32  *pbl2)
{
l_uint8  *data;
l_int32   redsize, nbytes;
PIX      *pix;
PIXA     *pixa;

    if (!pbl0 || !pbl1 || !pbl2)
        return (PIXA *)ERROR_PTR("&bl not all defined", __func__, NULL);
    *pbl0 = *pbl1 = *pbl2 = 0;
    redsize = (fontsize / 2) - 2;
    if (redsize < 0 || redsize >= NUM_FONTS)
        return (PIXA *)ERROR_PTR("invalid font size", __func__, NULL);

    if (fontsize == 4) {
        data = decodeBase64(fontdata_4, strlen(fontdata_4), &nbytes);
    } else if (fontsize == 6) {
        data = decodeBase64(fontdata_6, strlen(fontdata_6), &nbytes);
    } else if (fontsize == 8) {
        data = decodeBase64(fontdata_8, strlen(fontdata_8), &nbytes);
    } else if (fontsize == 10) {
        data = decodeBase64(fontdata_10, strlen(fontdata_10), &nbytes);
    } else if (fontsize == 12) {
        data = decodeBase64(fontdata_12, strlen(fontdata_12), &nbytes);
    } else if (fontsize == 14) {
        data = decodeBase64(fontdata_14, strlen(fontdata_14), &nbytes);
    } else if (fontsize == 16) {
        data = decodeBase64(fontdata_16, strlen(fontdata_16), &nbytes);
    } else if (fontsize == 18) {
        data = decodeBase64(fontdata_18, strlen(fontdata_18), &nbytes);
    } else {  /* fontsize == 20 */
        data = decodeBase64(fontdata_20, strlen(fontdata_20), &nbytes);
    }
    if (!data)
        return (PIXA *)ERROR_PTR("data not made", __func__, NULL);

    pix = pixReadMem(data, nbytes);
    LEPT_FREE(data);
    if (!pix)
        return (PIXA *)ERROR_PTR("pix not made", __func__, NULL);

    pixa = pixaGenerateFont(pix, fontsize, pbl0, pbl1, pbl2);
    pixDestroy(&pix);
    return pixa;
}


/*!
 * \brief   pixaGenerateFont()
 *
 * \param[in]    pixs       of 95 characters in 3 rows
 * \param[in]    fontsize   4, 6, 8, ... , 20, in pts at 300 ppi
 * \param[out]   pbl0       baseline of row 1
 * \param[out]   pbl1       baseline of row 2
 * \param[out]   pbl2       baseline of row 3
 * \return  pixa   of font bitmaps for 95 characters, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This does all the work.  See pixaGenerateFontFromFile()
 *          for an overview.
 *      (2) The pix is for one of the 9 fonts.  %fontsize is only
 *          used here for debugging.
 * </pre>
 */
static PIXA *
pixaGenerateFont(PIX      *pixs,
                 l_int32   fontsize,
                 l_int32  *pbl0,
                 l_int32  *pbl1,
                 l_int32  *pbl2)
{
l_int32   i, j, nrows, nrowchars, nchars, h, yval;
l_int32   width, height;
l_int32   baseline[3];
l_int32  *tab = NULL;
BOX      *box, *box1, *box2;
BOXA     *boxar, *boxac, *boxacs;
PIX      *pix1, *pix2, *pixr, *pixrc, *pixc;
PIXA     *pixa;
l_int32   n, w, inrow, top;
l_int32  *ia;
NUMA     *na;

    if (!pbl0 || !pbl1 || !pbl2)
        return (PIXA *)ERROR_PTR("&bl not all defined", __func__, NULL);
    *pbl0 = *pbl1 = *pbl2 = 0;
    if (!pixs)
        return (PIXA *)ERROR_PTR("pixs not defined", __func__, NULL);

        /* Locate the 3 rows of characters */
    w = pixGetWidth(pixs);
    na = pixCountPixelsByRow(pixs, NULL);
    boxar = boxaCreate(0);
    n = numaGetCount(na);
    ia = numaGetIArray(na);
    inrow = 0;
    for (i = 0; i < n; i++) {
        if (!inrow && ia[i] > 0) {
            inrow = 1;
            top = i;
        } else if (inrow && ia[i] == 0) {
            inrow = 0;
            box = boxCreate(0, top, w, i - top);
            boxaAddBox(boxar, box, L_INSERT);
        }
    }
    LEPT_FREE(ia);
    numaDestroy(&na);
    nrows = boxaGetCount(boxar);
#if  DEBUG_FONT_GEN
    L_INFO("For fontsize %s, have %d rows\n", __func__, fontsize, nrows);
#endif  /* DEBUG_FONT_GEN */
    if (nrows != 3) {
        L_INFO("nrows = %d; skipping fontsize %d\n", __func__, nrows, fontsize);
        boxaDestroy(&boxar);
        return (PIXA *)ERROR_PTR("3 rows not generated", __func__, NULL);
    }

        /* Grab the character images and baseline data */
#if DEBUG_BASELINE
    lept_rmdir("baseline");
    lept_mkdir("baseline");
#endif  /* DEBUG_BASELINE */
    tab = makePixelSumTab8();
    pixa = pixaCreate(95);
    for (i = 0; i < nrows; i++) {
        box = boxaGetBox(boxar, i, L_CLONE);
        pixr = pixClipRectangle(pixs, box, NULL);  /* row of chars */
        pixGetTextBaseline(pixr, tab, &yval);
        baseline[i] = yval;

#if DEBUG_BASELINE
        L_INFO("Baseline info: row %d, yval = %d, h = %d\n", __func__,
               i, yval, pixGetHeight(pixr));
        pix1 = pixCopy(NULL, pixr);
        pixRenderLine(pix1, 0, yval, pixGetWidth(pix1), yval, 1,
                      L_FLIP_PIXELS);
        if (i == 0 )
            pixWriteDebug("/tmp/baseline/row0.png", pix1, IFF_PNG);
        else if (i == 1)
            pixWriteDebug("/tmp/baseline/row1.png", pix1, IFF_PNG);
        else
            pixWriteDebug("/tmp/baseline/row2.png", pix1, IFF_PNG);
        pixDestroy(&pix1);
#endif  /* DEBUG_BASELINE */

        boxDestroy(&box);
        pixrc = pixCloseSafeBrick(NULL, pixr, 1, 35);
        boxac = pixConnComp(pixrc, NULL, 8);
        boxacs = boxaSort(boxac, L_SORT_BY_X, L_SORT_INCREASING, NULL);
        if (i == 0) {  /* consolidate the two components of '"' */
            box1 = boxaGetBox(boxacs, 1, L_CLONE);
            box2 = boxaGetBox(boxacs, 2, L_CLONE);
            box1->w = box2->x + box2->w - box1->x;  /* increase width */
            boxDestroy(&box1);
            boxDestroy(&box2);
            boxaRemoveBox(boxacs, 2);
        }
        h = pixGetHeight(pixr);
        nrowchars = boxaGetCount(boxacs);
        for (j = 0; j < nrowchars; j++) {
            box = boxaGetBox(boxacs, j, L_COPY);
            if (box->w <= 2 && box->h == 1) {  /* skip 1x1, 2x1 components */
                boxDestroy(&box);
                continue;
            }
            box->y = 0;
            box->h = h - 1;
            pixc = pixClipRectangle(pixr, box, NULL);
            boxDestroy(&box);
            if (i == 0 && j == 0)  /* add a pix for the space; change later */
                pixaAddPix(pixa, pixc, L_COPY);
            if (i == 2 && j == 0)  /* add a pix for the '\'; change later */
                pixaAddPix(pixa, pixc, L_COPY);
            pixaAddPix(pixa, pixc, L_INSERT);
        }
        pixDestroy(&pixr);
        pixDestroy(&pixrc);
        boxaDestroy(&boxac);
        boxaDestroy(&boxacs);
    }
    LEPT_FREE(tab);

    nchars = pixaGetCount(pixa);
    if (nchars != 95)
        return (PIXA *)ERROR_PTR("95 chars not generated", __func__, NULL);

    *pbl0 = baseline[0];
    *pbl1 = baseline[1];
    *pbl2 = baseline[2];

        /* Fix the space character up; it should have no ON pixels,
         * and be about twice as wide as the '!' character.    */
    pix1 = pixaGetPix(pixa, 0, L_CLONE);
    width = 2 * pixGetWidth(pix1);
    height = pixGetHeight(pix1);
    pixDestroy(&pix1);
    pix1 = pixCreate(width, height, 1);
    pixaReplacePix(pixa, 0, pix1, NULL);

        /* Fix up the '\' character; use a LR flip of the '/' char */
    pix1 = pixaGetPix(pixa, 15, L_CLONE);
    pix2 = pixFlipLR(NULL, pix1);
    pixDestroy(&pix1);
    pixaReplacePix(pixa, 60, pix2, NULL);

#if DEBUG_CHARS
    pix1 = pixaDisplayTiled(pixa, 1500, 0, 10);
    pixDisplay(pix1, 100 * i, 200);
    pixDestroy(&pix1);
#endif  /* DEBUG_CHARS */

    boxaDestroy(&boxar);
    return pixa;
}


/*!
 * \brief   pixGetTextBaseline()
 *
 * \param[in]    pixs    1 bpp, one textline character set
 * \param[in]    tab8    [optional] pixel sum table
 * \param[out]   py      baseline value
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Method: find the largest difference in pixel sums from one
 *          raster line to the next one below it.  The baseline is the
 *          upper raster line for the pair of raster lines that
 *          maximizes this function.
 * </pre>
 */
static l_int32
pixGetTextBaseline(PIX      *pixs,
                   l_int32  *tab8,
                   l_int32  *py)
{
l_int32   i, h, val1, val2, diff, diffmax, ymax;
l_int32  *tab;
NUMA     *na;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (!py)
        return ERROR_INT("&y not defined", __func__, 1);
    *py = 0;
    if (!tab8)
        tab = makePixelSumTab8();
    else
        tab = tab8;

    na = pixCountPixelsByRow(pixs, tab);
    h = numaGetCount(na);
    diffmax = 0;
    ymax = 0;
    for (i = 1; i < h; i++) {
        numaGetIValue(na, i - 1, &val1);
        numaGetIValue(na, i, &val2);
        diff = L_MAX(0, val1 - val2);
        if (diff > diffmax) {
            diffmax = diff;
            ymax = i - 1;  /* upper raster line */
        }
    }
    *py = ymax;

    if (!tab8)
        LEPT_FREE(tab);
    numaDestroy(&na);
    return 0;
}


/*!
 * \brief   bmfMakeAsciiTables
 *
 * \param[in]    bmf
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This makes two tables, each of size 128, as follows:
 *          ~ fonttab is a table containing the index of the Pix
 *            that corresponds to each input ascii character;
 *            it maps (ascii-index) --> Pixa index
 *          ~ baselinetab is a table containing the baseline offset
 *            for the Pix that corresponds to each input ascii character;
 *            it maps (ascii-index) --> baseline offset
 *     (2) This also computes
 *          ~ lineheight (sum of maximum character extensions above and
 *                        below the baseline)
 *          ~ kernwidth (spacing between characters within a word)
 *          ~ spacewidth (space between words)
 *          ~ vertlinesep (extra vertical spacing between textlines)
 *     (3) The baselines apply as follows:
 *          baseline1   (ascii 32 - 57), ascii 92
 *          baseline2   (ascii 58 - 91)
 *          baseline3   (ascii 93 - 126)
 *     (4) The only array in bmf that is not ascii-based is the
 *         array of bitmaps in the pixa, which starts at ascii 32.
 * </pre>
 */
static l_int32
bmfMakeAsciiTables(L_BMF  *bmf)
{
l_int32   i, maxh, height, charwidth, xwidth, kernwidth;
l_int32  *fonttab, *baselinetab;
PIX      *pix;

    if (!bmf)
        return ERROR_INT("bmf not defined", __func__, 1);

        /* First get the fonttab; we use this later for the char widths */
    fonttab = (l_int32 *)LEPT_CALLOC(128, sizeof(l_int32));
    bmf->fonttab = fonttab;
    for (i = 0; i < 128; i++)
        fonttab[i] = UNDEF;
    for (i = 32; i < 127; i++)
        fonttab[i] = i - 32;

    baselinetab = (l_int32 *)LEPT_CALLOC(128, sizeof(l_int32));
    bmf->baselinetab = baselinetab;
    for (i = 0; i < 128; i++)
        baselinetab[i] = UNDEF;
    for (i = 32; i <= 57; i++)
        baselinetab[i] = bmf->baseline1;
    for (i = 58; i <= 91; i++)
        baselinetab[i] = bmf->baseline2;
    baselinetab[92] = bmf->baseline1;  /* the '\' char */
    for (i = 93; i < 127; i++)
        baselinetab[i] = bmf->baseline3;

        /* Get the line height of text characters, from the highest
         * ascender to the lowest descender; req's fonttab to exist. */
    pix =  bmfGetPix(bmf, 32);
    maxh =  pixGetHeight(pix);
    pixDestroy(&pix);
    pix =  bmfGetPix(bmf, 58);
    height =  pixGetHeight(pix);
    pixDestroy(&pix);
    maxh = L_MAX(maxh, height);
    pix =  bmfGetPix(bmf, 93);
    height =  pixGetHeight(pix);
    pixDestroy(&pix);
    maxh = L_MAX(maxh, height);
    bmf->lineheight = maxh;

        /* Get the kern width (distance between characters).
         * We let it be the same for all characters in a given
         * font size, and scale it linearly with the size;
         * req's fonttab to be built first. */
    bmfGetWidth(bmf, 120, &xwidth);
    kernwidth = (l_int32)(0.08 * (l_float32)xwidth + 0.5);
    bmf->kernwidth = L_MAX(1, kernwidth);

        /* Save the space width (between words) */
    bmfGetWidth(bmf, 32, &charwidth);
    bmf->spacewidth = charwidth;

        /* Save the extra vertical space between lines */
    bmf->vertlinesep = (l_int32)(VertFractSep * bmf->lineheight + 0.5);

    return 0;
}
