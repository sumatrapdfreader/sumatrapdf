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
 * \file textops.c
 * <pre>
 *
 *    Font layout
 *       PIX             *pixAddSingleTextblock()
 *       PIX             *pixAddTextlines()
 *       l_int32          pixSetTextblock()
 *       l_int32          pixSetTextline()
 *       PIXA            *pixaAddTextNumber()
 *       PIXA            *pixaAddTextlines()
 *       l_int32          pixaAddPixWithText()
 *
 *    Text size estimation and partitioning
 *       SARRAY          *bmfGetLineStrings()
 *       NUMA            *bmfGetWordWidths()
 *       l_int32          bmfGetStringWidth()
 *
 *    Text splitting
 *       SARRAY          *splitStringToParagraphs()
 *       static l_int32   stringAllWhitespace()
 *       static l_int32   stringLeadingWhitespace()
 *
 *    This is a simple utility to put text on images.  One font and style
 *    is provided, with a variety of pt sizes.  For example, to put a
 *    line of green 10 pt text on an image, with the beginning baseline
 *    at (50, 50):
 *        L_Bmf  *bmf = bmfCreate(NULL, 10);
 *        const char *textstr = "This is a funny cat";
 *        pixSetTextline(pixs, bmf, textstr, 0x00ff0000, 50, 50, NULL, NULL);
 *
 *    The simplest interfaces for adding text to an image are
 *    pixAddTextlines() and pixAddSingleTextblock().
 *    For example, to add the same text in red, centered, below the image:
 *        Pix *pixd = pixAddTextlines(pixs, bmf, textstr, 0xff000000,
 *                                    L_ADD_BELOW);  // red text
 *
 *    To add text to all pix in a pixa, generating a new pixa, use
 *    either an sarray to hold the strings for each pix, or use the
 *    strings in the text field of each pix; e.g.,
 *        Pixa *pixa2 = pixaAddTextlines(pixa1, bmf, sa, 0x0000ff00,
 *                                    L_ADD_LEFT);  // blue text
 *        Pixa *pixa2 = pixaAddTextlines(pixa1, bmf, NULL, 0x00ff0000,
 *                                    L_ADD_RIGHT);  // green text
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"

static l_int32 stringAllWhitespace(char *textstr, l_int32 *pval);
static l_int32 stringLeadingWhitespace(char *textstr, l_int32 *pval);


/*---------------------------------------------------------------------*
 *                                 Font layout                         *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixAddSingleTextblock()
 *
 * \param[in]    pixs        input pix; colormap ok
 * \param[in]    bmf         bitmap font data
 * \param[in]    textstr     [optional] text string to be added
 * \param[in]    val         color to set the text
 * \param[in]    location    L_ADD_ABOVE, L_ADD_AT_TOP,
 *                           L_ADD_AT_BOT, L_ADD_BELOW
 * \param[out]   poverflow   [optional] 1 if text overflows allocated
 *                           region and is clipped; 0 otherwise
 * \return  pixd   new pix with rendered text, or either a copy,
 *                 or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This function paints a set of lines of text over an image.
 *          If %location is L_ADD_ABOVE or L_ADD_BELOW, the pix size
 *          is expanded with a border and rendered over the border.
 *      (2) %val is the pixel value to be painted through the font mask.
 *          It should be chosen to agree with the depth of pixs.
 *          If it is out of bounds, an intermediate value is chosen.
 *          For RGB, use hex notation: 0xRRGGBB00, where RR is the
 *          hex representation of the red intensity, etc.
 *      (3) If textstr == NULL, use the text field in the pix.
 *      (4) If there is a colormap, this does the best it can to use
 *          the requested color, or something similar to it.
 *      (5) Typical usage is for labelling a pix with some text data.
 * </pre>
 */
PIX *
pixAddSingleTextblock(PIX         *pixs,
                      L_BMF       *bmf,
                      const char  *textstr,
                      l_uint32     val,
                      l_int32      location,
                      l_int32     *poverflow)
{
char     *linestr;
l_int32   w, h, d, i, y, xstart, ystart, extra, spacer, rval, gval, bval;
l_int32   nlines, htext, ovf, overflow, offset, index;
l_uint32  textcolor;
PIX      *pixd;
PIXCMAP  *cmap, *cmapd;
SARRAY   *salines;

    if (poverflow) *poverflow = 0;
    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (location != L_ADD_ABOVE && location != L_ADD_AT_TOP &&
        location != L_ADD_AT_BOT && location != L_ADD_BELOW)
        return (PIX *)ERROR_PTR("invalid location", __func__, NULL);
    if (!bmf) {
        L_ERROR("no bitmap fonts; returning a copy\n", __func__);
        return pixCopy(NULL, pixs);
    }
    if (!textstr)
        textstr = pixGetText(pixs);
    if (!textstr) {
        L_WARNING("no textstring defined; returning a copy\n", __func__);
        return pixCopy(NULL, pixs);
    }

        /* Make sure the "color" value for the text will work
         * for the pix.  If the pix is not colormapped and the
         * value is out of range, set it to mid-range. */
    pixGetDimensions(pixs, &w, &h, &d);
    cmap = pixGetColormap(pixs);
    if (d == 1 && val > 1)
        val = 1;
    else if (d == 2 && val > 3 && !cmap)
        val = 2;
    else if (d == 4 && val > 15 && !cmap)
        val = 8;
    else if (d == 8 && val > 0xff && !cmap)
        val = 128;
    else if (d == 16 && val > 0xffff)
        val = 0x8000;
    else if (d == 32 && val < 256)
        val = 0x80808000;

    xstart = (l_int32)(0.1 * w);
    salines = bmfGetLineStrings(bmf, textstr, w - 2 * xstart, 0, &htext);
    if (!salines)
        return (PIX *)ERROR_PTR("line string sa not made", __func__, NULL);
    nlines = sarrayGetCount(salines);

        /* Add white border if required */
    spacer = 10;  /* pixels away from image boundary or added border */
    if (location == L_ADD_ABOVE || location == L_ADD_BELOW) {
        extra = htext + 2 * spacer;
        pixd = pixCreate(w, h + extra, d);
        pixCopyColormap(pixd, pixs);
        pixCopyResolution(pixd, pixs);
        pixCopyText(pixd, pixs);
        pixSetBlackOrWhite(pixd, L_BRING_IN_WHITE);
        if (location == L_ADD_ABOVE)
            pixRasterop(pixd, 0, extra, w, h, PIX_SRC, pixs, 0, 0);
        else  /* add below */
            pixRasterop(pixd, 0, 0, w, h, PIX_SRC, pixs, 0, 0);
    } else {
        pixd = pixCopy(NULL, pixs);
    }
    cmapd = pixGetColormap(pixd);

        /* bmf->baselinetab[93] is the approximate distance from
         * the top of the tallest character to the baseline.  93 was chosen
         * at random, as all the baselines are essentially equal for
         * each character in a font. */
    offset = bmf->baselinetab[93];
    if (location == L_ADD_ABOVE || location == L_ADD_AT_TOP)
        ystart = offset + spacer;
    else if (location == L_ADD_AT_BOT)
        ystart = h - htext - spacer + offset;
    else   /* add below */
        ystart = h + offset + spacer;

        /* If cmapped, add the color if necessary to the cmap.  If the
         * cmap is full, use the nearest color to the requested color. */
    if (cmapd) {
        extractRGBValues(val, &rval, &gval, &bval);
        pixcmapAddNearestColor(cmapd, rval, gval, bval, &index);
        pixcmapGetColor(cmapd, index, &rval, &gval, &bval);
        composeRGBPixel(rval, gval, bval, &textcolor);
    } else {
        textcolor = val;
    }

        /* Keep track of overflow condition on line width */
    overflow = 0;
    for (i = 0, y = ystart; i < nlines; i++) {
        linestr = sarrayGetString(salines, i, L_NOCOPY);
        pixSetTextline(pixd, bmf, linestr, textcolor,
                       xstart, y, NULL, &ovf);
        y += bmf->lineheight + bmf->vertlinesep;
        if (ovf)
            overflow = 1;
    }

       /* Also consider vertical overflow where there is too much text to
        * fit inside the image: the cases L_ADD_AT_TOP and L_ADD_AT_BOT.
        *  The text requires a total of htext + 2 * spacer vertical pixels. */
    if (location == L_ADD_AT_TOP || location == L_ADD_AT_BOT) {
        if (h < htext + 2 * spacer)
            overflow = 1;
    }
    if (poverflow) *poverflow = overflow;

    sarrayDestroy(&salines);
    return pixd;
}


/*!
 * \brief   pixAddTextlines()
 *
 * \param[in]    pixs        input pix; colormap ok
 * \param[in]    bmf         bitmap font data
 * \param[in]    textstr     [optional] text string to be added
 * \param[in]    val         color to set the text
 * \param[in]    location    L_ADD_ABOVE, L_ADD_BELOW, L_ADD_LEFT, L_ADD_RIGHT
 * \return  pixd   new pix with rendered text, or either a copy,
 *                 or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This function expands an image as required to paint one or
 *          more lines of text adjacent to the image.  If %bmf == NULL,
 *          this returns a copy.  If above or below, the lines are
 *          centered with respect to the image; if left or right, they
 *          are left justified.
 *      (2) %val is the pixel value to be painted through the font mask.
 *          It should be chosen to agree with the depth of pixs.
 *          If it is out of bounds, an intermediate value is chosen.
 *          For RGB, use hex notation: 0xRRGGBB00, where RR is the
 *          hex representation of the red intensity, etc.
 *      (3) If textstr == NULL, use the text field in the pix.  The
 *          text field contains one or most "lines" of text, where newlines
 *          are used as line separators.
 *      (4) If there is a colormap, this does the best it can to use
 *          the requested color, or something similar to it.
 *      (5) Typical usage is for labelling a pix with some text data.
 * </pre>
 */
PIX *
pixAddTextlines(PIX         *pixs,
                L_BMF       *bmf,
                const char  *textstr,
                l_uint32     val,
                l_int32      location)
{
char     *str;
l_int32   i, w, h, d, rval, gval, bval, index;
l_int32   wline, wtext, htext, wadd, hadd, spacer, hbaseline, nlines;
l_uint32  textcolor;
PIX      *pixd;
PIXCMAP  *cmap, *cmapd;
SARRAY   *sa;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (location != L_ADD_ABOVE && location != L_ADD_BELOW &&
        location != L_ADD_LEFT && location != L_ADD_RIGHT)
        return (PIX *)ERROR_PTR("invalid location", __func__, NULL);
    if (!bmf) {
        L_ERROR("no bitmap fonts; returning a copy\n", __func__);
        return pixCopy(NULL, pixs);
    }
    if (!textstr) {
        textstr = pixGetText(pixs);
        if (!textstr) {
            L_WARNING("no textstring defined; returning a copy\n", __func__);
            return pixCopy(NULL, pixs);
        }
    }

        /* Make sure the "color" value for the text will work
         * for the pix.  If the pix is not colormapped and the
         * value is out of range, set it to mid-range. */
    pixGetDimensions(pixs, &w, &h, &d);
    cmap = pixGetColormap(pixs);
    if (d == 1 && val > 1)
        val = 1;
    else if (d == 2 && val > 3 && !cmap)
        val = 2;
    else if (d == 4 && val > 15 && !cmap)
        val = 8;
    else if (d == 8 && val > 0xff && !cmap)
        val = 128;
    else if (d == 16 && val > 0xffff)
        val = 0x8000;
    else if (d == 32 && val < 256)
        val = 0x80808000;

        /* Get the text in each line */
    sa = sarrayCreateLinesFromString(textstr, 0);
    nlines = sarrayGetCount(sa);

        /* Get the necessary text size */
    wtext = 0;
    for (i = 0; i < nlines; i++) {
        str = sarrayGetString(sa, i, L_NOCOPY);
        bmfGetStringWidth(bmf, str, &wline);
        if (wline > wtext)
            wtext = wline;
    }
    hbaseline = bmf->baselinetab[93];
    htext = 1.5 * hbaseline * nlines;

        /* Add white border */
    spacer = 10;  /* pixels away from the added border */
    if (location == L_ADD_ABOVE || location == L_ADD_BELOW) {
        hadd = htext + 2 * spacer;
        pixd = pixCreate(w, h + hadd, d);
        pixCopyColormap(pixd, pixs);
        pixCopyResolution(pixd, pixs);
        pixCopyText(pixd, pixs);
        pixSetBlackOrWhite(pixd, L_BRING_IN_WHITE);
        if (location == L_ADD_ABOVE)
            pixRasterop(pixd, 0, hadd, w, h, PIX_SRC, pixs, 0, 0);
        else  /* add below */
            pixRasterop(pixd, 0, 0, w, h, PIX_SRC, pixs, 0, 0);
    } else {  /*  L_ADD_LEFT or L_ADD_RIGHT */
        wadd = wtext + 2 * spacer;
        pixd = pixCreate(w + wadd, h, d);
        pixCopyColormap(pixd, pixs);
        pixCopyResolution(pixd, pixs);
        pixCopyText(pixd, pixs);
        pixSetBlackOrWhite(pixd, L_BRING_IN_WHITE);
        if (location == L_ADD_LEFT)
            pixRasterop(pixd, wadd, 0, w, h, PIX_SRC, pixs, 0, 0);
        else  /* add to right */
            pixRasterop(pixd, 0, 0, w, h, PIX_SRC, pixs, 0, 0);
    }

        /* If cmapped, add the color if necessary to the cmap.  If the
         * cmap is full, use the nearest color to the requested color. */
    cmapd = pixGetColormap(pixd);
    if (cmapd) {
        extractRGBValues(val, &rval, &gval, &bval);
        pixcmapAddNearestColor(cmapd, rval, gval, bval, &index);
        pixcmapGetColor(cmapd, index, &rval, &gval, &bval);
        composeRGBPixel(rval, gval, bval, &textcolor);
    } else {
        textcolor = val;
    }

        /* Add the text */
    for (i = 0; i < nlines; i++) {
        str = sarrayGetString(sa, i, L_NOCOPY);
        bmfGetStringWidth(bmf, str, &wtext);
        if (location == L_ADD_ABOVE)
            pixSetTextline(pixd, bmf, str, textcolor,
                           (w - wtext) / 2, spacer + hbaseline * (1 + 1.5 * i),
                           NULL, NULL);
        else if (location == L_ADD_BELOW)
            pixSetTextline(pixd, bmf, str, textcolor,
                           (w - wtext) / 2, h + spacer +
                           hbaseline * (1 + 1.5 * i), NULL, NULL);
        else if (location == L_ADD_LEFT)
            pixSetTextline(pixd, bmf, str, textcolor,
                           spacer, (h - htext) / 2 + hbaseline * (1 + 1.5 * i),
                           NULL, NULL);
        else  /* location == L_ADD_RIGHT */
            pixSetTextline(pixd, bmf, str, textcolor,
                           w + spacer, (h - htext) / 2 +
                           hbaseline * (1 + 1.5 * i), NULL, NULL);
    }

    sarrayDestroy(&sa);
    return pixd;
}


/*!
 * \brief   pixSetTextblock()
 *
 * \param[in]    pixs          input image
 * \param[in]    bmf           bitmap font data
 * \param[in]    textstr       block text string to be set
 * \param[in]    val           color to set the text
 * \param[in]    x0            left edge for each line of text
 * \param[in]    y0            baseline location for the first text line
 * \param[in]    wtext         max width of each line of generated text
 * \param[in]    firstindent   indentation of first line, in x-widths
 * \param[out]   poverflow     [optional] 0 if text is contained in input pix;
 *                             1 if it is clipped
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This function paints a set of lines of text over an image.
 *      (2) %val is the pixel value to be painted through the font mask.
 *          It should be chosen to agree with the depth of pixs.
 *          If it is out of bounds, an intermediate value is chosen.
 *          For RGB, use hex notation: 0xRRGGBB00, where RR is the
 *          hex representation of the red intensity, etc.
 *          The last two hex digits are 00 (byte value 0), assigned to
 *          the A component.  Note that, as usual, RGBA proceeds from
 *          left to right in the order from MSB to LSB (see pix.h
 *          for details).
 *      (3) If there is a colormap, this does the best it can to use
 *          the requested color, or something similar to it.
 * </pre>
 */
l_ok
pixSetTextblock(PIX         *pixs,
                L_BMF       *bmf,
                const char  *textstr,
                l_uint32     val,
                l_int32      x0,
                l_int32      y0,
                l_int32      wtext,
                l_int32      firstindent,
                l_int32     *poverflow)
{
char     *linestr;
l_int32   d, h, i, w, x, y, nlines, htext, xwidth, wline, ovf, overflow;
SARRAY   *salines;
PIXCMAP  *cmap;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (!bmf)
        return ERROR_INT("bmf not defined", __func__, 1);
    if (!textstr)
        return ERROR_INT("textstr not defined", __func__, 1);

        /* Make sure the "color" value for the text will work
         * for the pix.  If the pix is not colormapped and the
         * value is out of range, set it to mid-range. */
    pixGetDimensions(pixs, &w, &h, &d);
    cmap = pixGetColormap(pixs);
    if (d == 1 && val > 1)
        val = 1;
    else if (d == 2 && val > 3 && !cmap)
        val = 2;
    else if (d == 4 && val > 15 && !cmap)
        val = 8;
    else if (d == 8 && val > 0xff && !cmap)
        val = 128;
    else if (d == 16 && val > 0xffff)
        val = 0x8000;
    else if (d == 32 && val < 256)
        val = 0x80808000;

    if (w < x0 + wtext) {
        L_WARNING("reducing width of textblock\n", __func__);
        wtext = w - x0 - w / 10;
        if (wtext <= 0)
            return ERROR_INT("wtext too small; no room for text", __func__, 1);
    }

    salines = bmfGetLineStrings(bmf, textstr, wtext, firstindent, &htext);
    if (!salines)
        return ERROR_INT("line string sa not made", __func__, 1);
    nlines = sarrayGetCount(salines);
    bmfGetWidth(bmf, 'x', &xwidth);

    y = y0;
    overflow = 0;
    for (i = 0; i < nlines; i++) {
        if (i == 0)
            x = x0 + firstindent * xwidth;
        else
            x = x0;
        linestr = sarrayGetString(salines, i, L_NOCOPY);
        pixSetTextline(pixs, bmf, linestr, val, x, y, &wline, &ovf);
        y += bmf->lineheight + bmf->vertlinesep;
        if (ovf)
            overflow = 1;
    }

       /* (y0 - baseline) is the top of the printed text.  Character
        * 93 was chosen at random, as all the baselines are essentially
        * equal for each character in a font. */
    if (h < y0 - bmf->baselinetab[93] + htext)
        overflow = 1;
    if (poverflow)
        *poverflow = overflow;

    sarrayDestroy(&salines);
    return 0;
}


/*!
 * \brief   pixSetTextline()
 *
 * \param[in]    pixs        input image
 * \param[in]    bmf         bitmap font data
 * \param[in]    textstr     text string to be set on the line
 * \param[in]    val         color to set the text
 * \param[in]    x0          left edge for first char
 * \param[in]    y0          baseline location for all text on line
 * \param[out]   pwidth      [optional] width of generated text
 * \param[out]   poverflow   [optional] 0 if text is contained in input pix;
 *                           1 if it is clipped
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This function paints a line of text over an image.
 *      (2) %val is the pixel value to be painted through the font mask.
 *          It should be chosen to agree with the depth of pixs.
 *          If it is out of bounds, an intermediate value is chosen.
 *          For RGB, use hex notation: 0xRRGGBB00, where RR is the
 *          hex representation of the red intensity, etc.
 *          The last two hex digits are 00 (byte value 0), assigned to
 *          the A component.  Note that, as usual, RGBA proceeds from
 *          left to right in the order from MSB to LSB (see pix.h
 *          for details).
 *      (3) If there is a colormap, this does the best it can to use
 *          the requested color, or something similar to it.
 * </pre>
 */
l_ok
pixSetTextline(PIX         *pixs,
               L_BMF       *bmf,
               const char  *textstr,
               l_uint32     val,
               l_int32      x0,
               l_int32      y0,
               l_int32     *pwidth,
               l_int32     *poverflow)
{
char      chr;
l_int32   d, i, x, w, nchar, baseline, index, rval, gval, bval;
l_uint32  textcolor;
PIX      *pix;
PIXCMAP  *cmap;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (!bmf)
        return ERROR_INT("bmf not defined", __func__, 1);
    if (!textstr)
        return ERROR_INT("teststr not defined", __func__, 1);

    d = pixGetDepth(pixs);
    cmap = pixGetColormap(pixs);
    if (d == 1 && val > 1)
        val = 1;
    else if (d == 2 && val > 3 && !cmap)
        val = 2;
    else if (d == 4 && val > 15 && !cmap)
        val = 8;
    else if (d == 8 && val > 0xff && !cmap)
        val = 128;
    else if (d == 16 && val > 0xffff)
        val = 0x8000;
    else if (d == 32 && val < 256)
        val = 0x80808000;

        /* If cmapped, add the color if necessary to the cmap.  If the
         * cmap is full, use the nearest color to the requested color. */
    if (cmap) {
        extractRGBValues(val, &rval, &gval, &bval);
        pixcmapAddNearestColor(cmap, rval, gval, bval, &index);
        pixcmapGetColor(cmap, index, &rval, &gval, &bval);
        composeRGBPixel(rval, gval, bval, &textcolor);
    } else
        textcolor = val;

    nchar = strlen(textstr);
    x = x0;
    for (i = 0; i < nchar; i++) {
        chr = textstr[i];
        if ((l_int32)chr == 10) continue;  /* NL */
        pix = bmfGetPix(bmf, chr);
        bmfGetBaseline(bmf, chr, &baseline);
        pixPaintThroughMask(pixs, pix, x, y0 - baseline, textcolor);
        w = pixGetWidth(pix);
        x += w + bmf->kernwidth;
        pixDestroy(&pix);
    }

    if (pwidth)
        *pwidth = x - bmf->kernwidth - x0;
    if (poverflow)
        *poverflow = (x > pixGetWidth(pixs) - 1) ? 1 : 0;
    return 0;
}


/*!
 * \brief   pixaAddTextNumber()
 *
 * \param[in]    pixas      input pixa; colormap ok
 * \param[in]    bmf        bitmap font data
 * \param[in]    na         [optional] number array; use 1 ... n if null
 * \param[in]    val        color to set the text
 * \param[in]    location   L_ADD_ABOVE, L_ADD_BELOW, L_ADD_LEFT, L_ADD_RIGHT
 * \return  pixad   new pixa with rendered numbers, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Typical usage is for labelling each pix in a pixa with a number.
 *      (2) This function paints numbers external to each pix, in a position
 *          given by %location.  In all cases, the pix is expanded on
 *          on side and the number is painted over white in the added region.
 *      (3) %val is the pixel value to be painted through the font mask.
 *          It should be chosen to agree with the depth of pixs.
 *          If it is out of bounds, an intermediate value is chosen.
 *          For RGB, use hex notation: 0xRRGGBB00, where RR is the
 *          hex representation of the red intensity, etc.
 *      (4) If na == NULL, number each pix sequentially, starting with 1.
 *      (5) If there is a colormap, this does the best it can to use
 *          the requested color, or something similar to it.
 * </pre>
 */
PIXA *
pixaAddTextNumber(PIXA     *pixas,
                  L_BMF    *bmf,
                  NUMA     *na,
                  l_uint32  val,
                  l_int32   location)
{
char     textstr[128];
l_int32  i, n, index;
PIX     *pix1, *pix2;
PIXA    *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);
    if (!bmf)
        return (PIXA *)ERROR_PTR("bmf not defined", __func__, NULL);
    if (location != L_ADD_ABOVE && location != L_ADD_BELOW &&
        location != L_ADD_LEFT && location != L_ADD_RIGHT)
        return (PIXA *)ERROR_PTR("invalid location", __func__, NULL);

    n = pixaGetCount(pixas);
    pixad = pixaCreate(n);
    for (i = 0; i < n; i++) {
        pix1 = pixaGetPix(pixas, i, L_CLONE);
        if (na)
            numaGetIValue(na, i, &index);
        else
            index = i + 1;
        snprintf(textstr, sizeof(textstr), "%d", index);
        pix2 = pixAddTextlines(pix1, bmf, textstr, val, location);
        pixaAddPix(pixad, pix2, L_INSERT);
        pixDestroy(&pix1);
    }

    return pixad;
}


/*!
 * \brief   pixaAddTextlines()
 *
 * \param[in]    pixas      input pixa; colormap ok
 * \param[in]    bmf        bitmap font data
 * \param[in]    sa         [optional] sarray; use text embedded in
 *                          each pix if null
 * \param[in]    val        color to set the text
 * \param[in]    location   L_ADD_ABOVE, L_ADD_BELOW, L_ADD_LEFT, L_ADD_RIGHT
 * \return  pixad   new pixa with rendered text, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This function adds one or more lines of text externally to
 *          each pix, in a position given by %location.  In all cases,
 *          the pix is expanded as necessary to accommodate the text.
 *      (2) %val is the pixel value to be painted through the font mask.
 *          It should be chosen to agree with the depth of pixs.
 *          If it is out of bounds, an intermediate value is chosen.
 *          For RGB, use hex notation: 0xRRGGBB00, where RR is the
 *          hex representation of the red intensity, etc.
 *      (3) If sa == NULL, use the text embedded in each pix.  In all
 *          cases, newlines in the text string are used to separate the
 *          lines of text that are added to the pix.
 *      (4) If sa has a smaller count than pixa, issue a warning
 *          and do not use any embedded text.
 *      (5) If there is a colormap, this does the best it can to use
 *          the requested color, or something similar to it.
 * </pre>
 */
PIXA *
pixaAddTextlines(PIXA     *pixas,
                 L_BMF    *bmf,
                 SARRAY   *sa,
                 l_uint32  val,
                 l_int32   location)
{
char    *textstr;
l_int32  i, n, nstr;
PIX     *pix1, *pix2;
PIXA    *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);
    if (!bmf)
        return (PIXA *)ERROR_PTR("bmf not defined", __func__, NULL);
    if (location != L_ADD_ABOVE && location != L_ADD_BELOW &&
        location != L_ADD_LEFT && location != L_ADD_RIGHT)
        return (PIXA *)ERROR_PTR("invalid location", __func__, NULL);

    n = pixaGetCount(pixas);
    pixad = pixaCreate(n);
    nstr = (sa) ? sarrayGetCount(sa) : 0;
    if (nstr > 0 && nstr < n)
        L_WARNING("There are %d strings and %d pix\n", __func__, nstr, n);
    for (i = 0; i < n; i++) {
        pix1 = pixaGetPix(pixas, i, L_CLONE);
        if (i < nstr)
            textstr = sarrayGetString(sa, i, L_NOCOPY);
        else
            textstr = pixGetText(pix1);
        pix2 = pixAddTextlines(pix1, bmf, textstr, val, location);
        pixaAddPix(pixad, pix2, L_INSERT);
        pixDestroy(&pix1);
    }

    return pixad;
}


/*!
 * \brief   pixaAddPixWithText()
 *
 * \param[in]    pixa
 * \param[in]    pixs       any depth, colormap ok
 * \param[in]    reduction  integer subsampling factor
 * \param[in]    bmf        [optional] bitmap font data
 * \param[in]    textstr    [optional] text string to be added
 * \param[in]    val        color to set the text
 * \param[in]    location   L_ADD_ABOVE, L_ADD_BELOW, L_ADD_LEFT, L_ADD_RIGHT
 * \return  0 if OK, 1 on error.
 *
 * <pre>
 * Notes:
 *      (1) This function generates a new pix with added text, and adds
 *          it by insertion into the pixa.
 *      (2) If the input pixs is not cmapped and not 32 bpp, it is
 *          converted to 32 bpp rgb.  %val is a standard 32 bpp pixel,
 *          expressed as 0xrrggbb00.  If there is a colormap, this does
 *          the best it can to use the requested color, or something close.
 *      (3) if %bmf == NULL, generate an 8 pt font; this takes about 5 msec.
 *      (4) If %textstr == NULL, use the text field in the pix.
 *      (5) In general, the text string can be written in multiple lines;
 *          use newlines as the separators.
 *      (6) Typical usage is for debugging, where the pixa of labeled images
 *          is used to generate a pdf.  Suggest using 1.0 for scalefactor.
 * </pre>
 */
l_ok
pixaAddPixWithText(PIXA        *pixa,
                   PIX         *pixs,
                   l_int32      reduction,
                   L_BMF       *bmf,
                   const char  *textstr,
                   l_uint32     val,
                   l_int32      location)
{
l_int32   d;
L_BMF    *bmf8;
PIX      *pix1, *pix2, *pix3;
PIXCMAP  *cmap;

    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (location != L_ADD_ABOVE && location != L_ADD_BELOW &&
        location != L_ADD_LEFT && location != L_ADD_RIGHT)
        return ERROR_INT("invalid location", __func__, 1);

    if (!textstr) {
        textstr = pixGetText(pixs);
        if (!textstr) {
            L_WARNING("no textstring defined; inserting copy", __func__);
            pixaAddPix(pixa, pixs, L_COPY);
            return 0;
        }
    }

        /* Default font size is 8. */
    bmf8 = (bmf) ? bmf : bmfCreate(NULL, 8);

    if (reduction != 1)
        pix1 = pixScaleByIntSampling(pixs, reduction);
    else
        pix1 = pixClone(pixs);

        /* We want the text to be rendered in color.  This works
         * automatically if pixs is cmapped or 32 bpp rgb; otherwise,
         * we need to convert to rgb. */
    cmap = pixGetColormap(pix1);
    d = pixGetDepth(pix1);
    if (!cmap && d != 32)
        pix2 = pixConvertTo32(pix1);
    else
        pix2 = pixClone(pix1);

    pix3 = pixAddTextlines(pix2, bmf, textstr, val, location);
    pixDestroy(&pix1);
    pixDestroy(&pix2);
    if (!bmf) bmfDestroy(&bmf8);
    if (!pix3)
        return ERROR_INT("pix3 not made", __func__, 1);

    pixaAddPix(pixa, pix3, L_INSERT);
    return 0;
}


/*---------------------------------------------------------------------*
 *                   Text size estimation and partitioning             *
 *---------------------------------------------------------------------*/
/*!
 * \brief   bmfGetLineStrings()
 *
 * \param[in]    bmf
 * \param[in]    textstr
 * \param[in]    maxw          max width of a text line in pixels
 * \param[in]    firstindent   indentation of first line, in x-widths
 * \param[out]   ph            height required to hold text bitmap
 * \return  sarray of text strings for each line, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Divides the input text string into an array of text strings,
 *          each of which will fit within maxw bits of width.
 * </pre>
 */
SARRAY *
bmfGetLineStrings(L_BMF       *bmf,
                  const char  *textstr,
                  l_int32      maxw,
                  l_int32      firstindent,
                  l_int32     *ph)
{
char    *linestr;
l_int32  i, ifirst, sumw, newsum, w, nwords, nlines, len, xwidth;
NUMA    *na;
SARRAY  *sa, *sawords;

    if (!bmf)
        return (SARRAY *)ERROR_PTR("bmf not defined", __func__, NULL);
    if (!textstr)
        return (SARRAY *)ERROR_PTR("teststr not defined", __func__, NULL);

    if ((sawords = sarrayCreateWordsFromString(textstr)) == NULL)
        return (SARRAY *)ERROR_PTR("sawords not made", __func__, NULL);

    if ((na = bmfGetWordWidths(bmf, textstr, sawords)) == NULL) {
        sarrayDestroy(&sawords);
        return (SARRAY *)ERROR_PTR("na not made", __func__, NULL);
    }
    nwords = numaGetCount(na);
    if (nwords == 0) {
        sarrayDestroy(&sawords);
        numaDestroy(&na);
        return (SARRAY *)ERROR_PTR("no words in textstr", __func__, NULL);
    }
    bmfGetWidth(bmf, 'x', &xwidth);

    sa = sarrayCreate(0);
    ifirst = 0;
    numaGetIValue(na, 0, &w);
    sumw = firstindent * xwidth + w;
    for (i = 1; i < nwords; i++) {
        numaGetIValue(na, i, &w);
        newsum = sumw + bmf->spacewidth + w;
        if (newsum > maxw) {
            linestr = sarrayToStringRange(sawords, ifirst, i - ifirst, 2);
            if (!linestr)
                continue;
            len = strlen(linestr);
            if (len > 0)  /* it should always be */
                linestr[len - 1] = '\0';  /* remove the last space */
            sarrayAddString(sa, linestr, L_INSERT);
            ifirst = i;
            sumw = w;
        }
        else
            sumw += bmf->spacewidth + w;
    }
    linestr = sarrayToStringRange(sawords, ifirst, nwords - ifirst, 2);
    if (linestr)
        sarrayAddString(sa, linestr, L_INSERT);
    nlines = sarrayGetCount(sa);
    *ph = nlines * bmf->lineheight + (nlines - 1) * bmf->vertlinesep;

    sarrayDestroy(&sawords);
    numaDestroy(&na);
    return sa;
}


/*!
 * \brief   bmfGetWordWidths()
 *
 * \param[in]    bmf
 * \param[in]    textstr
 * \param[in]    sa        of individual words
 * \return  numa  of word lengths in pixels for the font represented
 *                by the bmf, or NULL on error
 */
NUMA *
bmfGetWordWidths(L_BMF       *bmf,
                 const char  *textstr,
                 SARRAY      *sa)
{
char    *wordstr;
l_int32  i, nwords, width;
NUMA    *na;

    if (!bmf)
        return (NUMA *)ERROR_PTR("bmf not defined", __func__, NULL);
    if (!textstr)
        return (NUMA *)ERROR_PTR("teststr not defined", __func__, NULL);
    if (!sa)
        return (NUMA *)ERROR_PTR("sa not defined", __func__, NULL);

    nwords = sarrayGetCount(sa);
    if ((na = numaCreate(nwords)) == NULL)
        return (NUMA *)ERROR_PTR("na not made", __func__, NULL);

    for (i = 0; i < nwords; i++) {
        wordstr = sarrayGetString(sa, i, L_NOCOPY);
        bmfGetStringWidth(bmf, wordstr, &width);
        numaAddNumber(na, width);
    }

    return na;
}


/*!
 * \brief   bmfGetStringWidth()
 *
 * \param[in]    bmf
 * \param[in]    textstr
 * \param[out]   pw        width of text string, in pixels for the
 *                         font represented by the bmf
 * \return  0 if OK, 1 on error
 */
l_ok
bmfGetStringWidth(L_BMF       *bmf,
                  const char  *textstr,
                  l_int32     *pw)
{
char     chr;
l_int32  i, w, width, nchar;

    if (!bmf)
        return ERROR_INT("bmf not defined", __func__, 1);
    if (!textstr)
        return ERROR_INT("teststr not defined", __func__, 1);
    if (!pw)
        return ERROR_INT("&w not defined", __func__, 1);

    nchar = strlen(textstr);
    w = 0;
    for (i = 0; i < nchar; i++) {
        chr = textstr[i];
        bmfGetWidth(bmf, chr, &width);
        if (width != UNDEF)
            w += width + bmf->kernwidth;
    }
    w -= bmf->kernwidth;  /* remove last one */

    *pw = w;
    return 0;
}



/*---------------------------------------------------------------------*
 *                             Text splitting                          *
 *---------------------------------------------------------------------*/
/*!
 * \brief   splitStringToParagraphs()
 *
 * \param[in]    textstr     text string
 * \param[in]    splitflag   see enum in bmf.h; valid values in {1,2,3}
 * \return  sarray  where each string is a paragraph of the input,
 *                  or NULL on error.
 */
SARRAY *
splitStringToParagraphs(char    *textstr,
                        l_int32  splitflag)
{
char    *linestr, *parastring;
l_int32  nlines, i, allwhite, leadwhite;
SARRAY  *salines, *satemp, *saout;

    if (!textstr)
        return (SARRAY *)ERROR_PTR("textstr not defined", __func__, NULL);

    if ((salines = sarrayCreateLinesFromString(textstr, 1)) == NULL)
        return (SARRAY *)ERROR_PTR("salines not made", __func__, NULL);
    nlines = sarrayGetCount(salines);
    saout = sarrayCreate(0);
    satemp = sarrayCreate(0);

    linestr = sarrayGetString(salines, 0, L_NOCOPY);
    sarrayAddString(satemp, linestr, L_COPY);
    for (i = 1; i < nlines; i++) {
        linestr = sarrayGetString(salines, i, L_NOCOPY);
        stringAllWhitespace(linestr, &allwhite);
        stringLeadingWhitespace(linestr, &leadwhite);
        if ((splitflag == SPLIT_ON_LEADING_WHITE && leadwhite) ||
            (splitflag == SPLIT_ON_BLANK_LINE && allwhite) ||
            (splitflag == SPLIT_ON_BOTH && (allwhite || leadwhite))) {
            parastring = sarrayToString(satemp, 1);  /* add nl to each line */
            sarrayAddString(saout, parastring, L_INSERT);
            sarrayDestroy(&satemp);
            satemp = sarrayCreate(0);
        }
        sarrayAddString(satemp, linestr, L_COPY);
    }
    parastring = sarrayToString(satemp, 1);  /* add nl to each line */
    sarrayAddString(saout, parastring, L_INSERT);
    sarrayDestroy(&satemp);
    sarrayDestroy(&salines);
    return saout;
}


/*!
 * \brief   stringAllWhitespace()
 *
 * \param[in]    textstr   text string
 * \param[out]   pval      1 if all whitespace; 0 otherwise
 * \return  0 if OK, 1 on error
 */
static l_int32
stringAllWhitespace(char     *textstr,
                    l_int32  *pval)
{
l_int32  len, i;

    if (!textstr)
        return ERROR_INT("textstr not defined", __func__, 1);
    if (!pval)
        return ERROR_INT("&va not defined", __func__, 1);

    len = strlen(textstr);
    *pval = 1;
    for (i = 0; i < len; i++) {
        if (textstr[i] != ' ' && textstr[i] != '\t' && textstr[i] != '\n') {
            *pval = 0;
            return 0;
        }
    }
    return 0;
}


/*!
 * \brief   stringLeadingWhitespace()
 *
 * \param[in]    textstr   text string
 * \param[out]   pval      1 if leading char is [space] or [tab]; 0 otherwise
 * \return  0 if OK, 1 on error
 */
static l_int32
stringLeadingWhitespace(char     *textstr,
                        l_int32  *pval)
{
    if (!textstr)
        return ERROR_INT("textstr not defined", __func__, 1);
    if (!pval)
        return ERROR_INT("&va not defined", __func__, 1);

    *pval = 0;
    if (textstr[0] == ' ' || textstr[0] == '\t')
        *pval = 1;

    return 0;
}
