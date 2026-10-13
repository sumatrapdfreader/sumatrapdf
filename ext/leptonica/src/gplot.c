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
 * \file gplot.c
 * <pre>
 *
 *     Basic plotting functions
 *          GPLOT      *gplotCreate()
 *          void        gplotDestroy()
 *          l_int32     gplotAddPlot()
 *          l_int32     gplotSetScaling()
 *          PIX        *gplotMakeOutputPix()
 *          l_int32     gplotMakeOutput()
 *          l_int32     gplotGenCommandFile()
 *          l_int32     gplotGenDataFiles()
 *
 *     Quick, one-line plots
 *          l_int32     gplotSimple1()
 *          l_int32     gplotSimple2()
 *          l_int32     gplotSimpleN()
 *          PIX        *gplotSimplePix1()
 *          PIX        *gplotSimplePix2()
 *          PIX        *gplotSimplePixN()
 *          GPLOT      *gplotSimpleXY1()
 *          GPLOT      *gplotSimpleXY2()
 *          GPLOT      *gplotSimpleXYN()
 *          PIX        *gplotGeneralPix1()
 *          PIX        *gplotGeneralPix2()
 *          PIX        *gplotGeneralPixN()
 *
 *     Serialize for I/O
 *          GPLOT      *gplotRead()
 *          l_int32     gplotWrite()
 *
 *
 *     Utility for programmatic plotting using gnuplot 4.6 or later
 *     Enabled:
 *         ~ output to png (color), ps and eps (mono), latex (mono)
 *         ~ optional title for plot
 *         ~ optional x and y axis labels
 *         ~ multiple plots on one frame
 *         ~ optional label for each plot on the frame
 *         ~ optional log scaling on either or both axes
 *         ~ choice of 5 plot styles for each array of input data
 *         ~ choice of 2 plot modes, either using one input array
 *           (Y vs index) or two input arrays (Y vs X).  For functions
 *           that take two arrays, the first mode (Y vs index) is
 *           employed if the first array is NULL.
 *
 *     General usage:
 *         gplotCreate() initializes for plotting
 *         gplotAddPlot() for each plot on the frame
 *         gplotMakeOutput() to generate all output files and run gnuplot
 *         gplotDestroy() to clean up
 *
 *     Example of use:
 *         gplot = gplotCreate("tempskew", GPLOT_PNG, "Skew score vs angle",
 *                    "angle (deg)", "score");
 *         gplotAddPlot(gplot, natheta, nascore1, GPLOT_LINES, "plot 1");
 *         gplotAddPlot(gplot, natheta, nascore2, GPLOT_POINTS, "plot 2");
 *         gplotSetScaling(gplot, GPLOT_LOG_SCALE_Y);
 *         gplotMakeOutput(gplot);
 *         gplotDestroy(&gplot);
 *
 *     Example usage of one-line plot generators:
 *
 *         -- Simple plots --
 *         Specify the root of output files, the output format,
 *         and the title (optional), but not the x and y coordinate labels
 *         or the plot labels.  The plotstyle defaults to GPLOT_LINES.
 *            gplotSimple2(na1, na2, GPLOT_PNG, "/tmp/lept/histo/gray",
 *                         "gray histogram");
 *         Multiple plots can be generated using gplotSimpleN().
 *
 *         -- Simple plots with more options --
 *         Specify the root of output files, the plotstyle, the output format,
 *         and optionally the title, but not the x and y coordinate labels
 *         or the plot labels.
 *            gplotSimpleXY1(na1, na2, GPLOT_LINES, GPLOT_PNG,
 *                           "/tmp/lept/histo/gray", "gray histogram");
 *         Multiple plots can be generated using gplotSimpleXYN().
 *
 *         -- Simple plots returning a pix --
 *         Specify only the title (optional).  The plotstyle defaults
 *         GPLOT_LINES and the output format is GPLOT_PNG..
 *         You can't specify the x and y coordinate labels or the plot label.
 *         The rootname of the generated files is determined internally.
 *            Pix *pix = gplotSimplePix2(na1, na2, "gray histogram");
 *         Multiple plots can be generated using gplotSimplePixN().
 *
 *         -- General plots returning a pix --
 *         Specify the root of the output files, the plotstyle, and optionally
 *         the title and axis labels.  This does not allow the individual
 *         plots to have plot labels, or to use different plotstyles
 *         for each plot.
 *            Pix *pix = gplotGeneralPix2(na1, na2, "/tmp/lept/histo/gray",
 *                                   GPLOT_LINES, "gray histogram",
 *                                   "pix value", "num pixels");
 *         Multiple plots can be generated using gplotGeneralPixN().
 *
 *     Note for output to GPLOT_LATEX:
 *         This creates latex output of the plot, named <rootname>.tex.
 *         It needs to be placed in a latex file <latexname>.tex
 *         that precedes the plot output with, at a minimum:
 *           \documentclass{article}
 *           \begin{document}
 *         and ends with
 *           \end{document}
 *         You can then generate a dvi file <latexname>.dvi using
 *           latex <latexname>.tex
 *         a PostScript file <psname>.ps from that using
 *           dvips -o <psname>.ps <latexname>.dvi
 *         and pdf file <psname>.pdf from that using Ghostscript's ps2pdf:
 *           ps2pdf <psname>.ps <pdfname>.pdf
 *
 *     N.B. To generate plots:
 *          (1) It is necessary to have gnuplot installed on your Unix system,
 *              or wgnuplot on Windows.
 *          (2) You must enable debug operations:
 *                setLeptDebugOK(1);
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"

#define Bufsize 512  /* hardcoded below in fscanf */

const char  *gplotstylenames[] = {"with lines",
                                  "with points",
                                  "with impulses",
                                  "with linespoints",
                                  "with dots"};
const char  *gplotfileoutputs[] = {"",
                                   "PNG",
                                   "PS",
                                   "EPS",
                                   "LATEX",
                                   "PNM"};


/*-----------------------------------------------------------------*
 *                       Basic Plotting Functions                  *
 *-----------------------------------------------------------------*/
/*!
 * \brief   gplotCreate()
 *
 * \param[in]    rootname    root for all output files
 * \param[in]    outformat   GPLOT_PNG, GPLOT_PS, GPLOT_EPS,
 *                           GPLOT_LATEX, GPLOT_PNM
 * \param[in]    title       [optional] overall title
 * \param[in]    xlabel      [optional] x axis label
 * \param[in]    ylabel      [optional] y axis label
 * \return  gplot, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This initializes the plot.
 *      (2) The 'title', 'xlabel' and 'ylabel' strings can have spaces,
 *          double quotes and backquotes, but not single quotes.
 * </pre>
 */
GPLOT  *
gplotCreate(const char  *rootname,
            l_int32      outformat,
            const char  *title,
            const char  *xlabel,
            const char  *ylabel)
{
char    *newroot;
char     buf[Bufsize];
l_int32  badchar;
GPLOT   *gplot;

    if (!rootname)
        return (GPLOT *)ERROR_PTR("rootname not defined", __func__, NULL);
    if (outformat != GPLOT_PNG && outformat != GPLOT_PS &&
        outformat != GPLOT_EPS && outformat != GPLOT_LATEX &&
        outformat != GPLOT_PNM)
        return (GPLOT *)ERROR_PTR("outformat invalid", __func__, NULL);
    stringCheckForChars(rootname, "`;&|><\"?*$()", &badchar);
    if (badchar)  /* danger of command injection */
        return (GPLOT *)ERROR_PTR("invalid rootname", __func__, NULL);

#if !defined(HAVE_LIBPNG)
    if (outformat == GPLOT_PNG) {
        L_WARNING("png library missing; output pnm format\n", __func__);
        outformat = GPLOT_PNM;
    }
#endif

    gplot = (GPLOT *)LEPT_CALLOC(1, sizeof(GPLOT));
    gplot->cmddata = sarrayCreate(0);
    gplot->datanames = sarrayCreate(0);
    gplot->plotdata = sarrayCreate(0);
    gplot->plotlabels = sarrayCreate(0);
    gplot->plotstyles = numaCreate(0);

        /* Save title, labels, rootname, outformat, cmdname, outname */
    newroot = genPathname(rootname, NULL);
    gplot->rootname = newroot;
    gplot->outformat = outformat;
    snprintf(buf, Bufsize, "%s.cmd", rootname);
    gplot->cmdname = stringNew(buf);
    if (outformat == GPLOT_PNG)
        snprintf(buf, Bufsize, "%s.png", newroot);
    else if (outformat == GPLOT_PS)
        snprintf(buf, Bufsize, "%s.ps", newroot);
    else if (outformat == GPLOT_EPS)
        snprintf(buf, Bufsize, "%s.eps", newroot);
    else if (outformat == GPLOT_LATEX)
        snprintf(buf, Bufsize, "%s.tex", newroot);
    else if (outformat == GPLOT_PNM)
        snprintf(buf, Bufsize, "%s.pnm", newroot);
    gplot->outname = stringNew(buf);
    if (title) gplot->title = stringNew(title);
    if (xlabel) gplot->xlabel = stringNew(xlabel);
    if (ylabel) gplot->ylabel = stringNew(ylabel);

    return gplot;
}


/*!
 * \brief    gplotDestroy()
 *
 * \param[in,out]   pgplot    will be set to null before returning
 */
void
gplotDestroy(GPLOT  **pgplot)
{
GPLOT  *gplot;

    if (pgplot == NULL) {
        L_WARNING("ptr address is null!\n", __func__);
        return;
    }

    if ((gplot = *pgplot) == NULL)
        return;

    LEPT_FREE(gplot->rootname);
    LEPT_FREE(gplot->cmdname);
    sarrayDestroy(&gplot->cmddata);
    sarrayDestroy(&gplot->datanames);
    sarrayDestroy(&gplot->plotdata);
    sarrayDestroy(&gplot->plotlabels);
    numaDestroy(&gplot->plotstyles);
    LEPT_FREE(gplot->outname);
    if (gplot->title)
        LEPT_FREE(gplot->title);
    if (gplot->xlabel)
        LEPT_FREE(gplot->xlabel);
    if (gplot->ylabel)
        LEPT_FREE(gplot->ylabel);

    LEPT_FREE(gplot);
    *pgplot = NULL;
}


/*!
 * \brief   gplotAddPlot()
 *
 * \param[in]    gplot
 * \param[in]    nax         [optional] numa: set to null for Y_VS_I;
 *                           required for Y_VS_X
 * \param[in]    nay         numa; required for both Y_VS_I and Y_VS_X
 * \param[in]    plotstyle   GPLOT_LINES, GPLOT_POINTS, GPLOT_IMPULSES,
 *                           GPLOT_LINESPOINTS, GPLOT_DOTS
 * \param[in]    plotlabel   [optional] label for individual plot
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) There are 2 options for (x,y) values:
 *            o  To plot an array vs a linear function of the
 *               index, set %nax = NULL.
 *            o  To plot one array vs another, use both %nax and %nay.
 *      (2) If %nax is NULL, the x value corresponding to the i-th
 *          value of %nay is found from the startx and delx fields
 *          in %nay:
 *               x = startx + i * delx
 *          These are set with numaSetParameters().  Their default
 *          values are startx = 0.0, delx = 1.0.
 *      (3) If %nax is defined, it must be the same size as %nay, and
 *          must have at least one number.
 *      (4) The 'plotlabel' string can have spaces, double
 *          quotes and backquotes, but not single quotes.
 * </pre>
 */
l_ok
gplotAddPlot(GPLOT       *gplot,
             NUMA        *nax,
             NUMA        *nay,
             l_int32      plotstyle,
             const char  *plotlabel)
{
char       buf[Bufsize];
char       emptystring[] = "";
char      *datastr, *title;
l_int32    n, i;
l_float32  valx, valy, startx, delx;
SARRAY    *sa;

    if (!gplot)
        return ERROR_INT("gplot not defined", __func__, 1);
    if (!nay)
        return ERROR_INT("nay not defined", __func__, 1);
    if (plotstyle < 0 || plotstyle >= NUM_GPLOT_STYLES)
        return ERROR_INT("invalid plotstyle", __func__, 1);

    if ((n = numaGetCount(nay)) == 0)
        return ERROR_INT("no points to plot", __func__, 1);
    if (nax && (n != numaGetCount(nax)))
        return ERROR_INT("nax and nay sizes differ", __func__, 1);
    if (n == 1 && plotstyle == GPLOT_LINES) {
        L_INFO("only 1 pt; changing style to points\n", __func__);
        plotstyle = GPLOT_POINTS;
    }

        /* Save plotstyle and plotlabel */
    numaGetParameters(nay, &startx, &delx);
    numaAddNumber(gplot->plotstyles, plotstyle);
    if (plotlabel) {
        title = stringNew(plotlabel);
        sarrayAddString(gplot->plotlabels, title, L_INSERT);
    } else {
        sarrayAddString(gplot->plotlabels, emptystring, L_COPY);
    }

        /* Generate and save data filename */
    gplot->nplots++;
    snprintf(buf, Bufsize, "%s.data.%d", gplot->rootname, gplot->nplots);
    sarrayAddString(gplot->datanames, buf, L_COPY);

        /* Generate data and save as a string */
    sa = sarrayCreate(n);
    for (i = 0; i < n; i++) {
        if (nax)
            numaGetFValue(nax, i, &valx);
        else
            valx = startx + i * delx;
        numaGetFValue(nay, i, &valy);
        snprintf(buf, Bufsize, "%f %f\n", valx, valy);
        sarrayAddString(sa, buf, L_COPY);
    }
    datastr = sarrayToString(sa, 0);
    sarrayAddString(gplot->plotdata, datastr, L_INSERT);
    sarrayDestroy(&sa);

    return 0;
}


/*!
 * \brief   gplotSetScaling()
 *
 * \param[in]    gplot
 * \param[in]    scaling   GPLOT_LINEAR_SCALE, GPLOT_LOG_SCALE_X,
 *                         GPLOT_LOG_SCALE_Y, GPLOT_LOG_SCALE_X_Y
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) By default, the x and y axis scaling is linear.
 *      (2) Call this function to set semi-log or log-log scaling.
 * </pre>
 */
l_ok
gplotSetScaling(GPLOT   *gplot,
                l_int32  scaling)
{
    if (!gplot)
        return ERROR_INT("gplot not defined", __func__, 1);
    if (scaling != GPLOT_LINEAR_SCALE &&
        scaling != GPLOT_LOG_SCALE_X &&
        scaling != GPLOT_LOG_SCALE_Y &&
        scaling != GPLOT_LOG_SCALE_X_Y)
        return ERROR_INT("invalid gplot scaling", __func__, 1);
    gplot->scaling = scaling;
    return 0;
}


/*!
 * \brief   gplotMakeOutputPix()
 *
 * \param[in]    gplot
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This wraps gplotMakeOutput(), and returns a pix.
 *          See gplotMakeOutput() for details.
 *      (2) The gplot output format must be an image (png or pnm).
 * </pre>
 */
PIX *
gplotMakeOutputPix(GPLOT  *gplot)
{
    if (!gplot)
        return (PIX *)ERROR_PTR("gplot not defined", __func__, NULL);
    if (gplot->outformat != GPLOT_PNG && gplot->outformat != GPLOT_PNM)
        return (PIX *)ERROR_PTR("output format not an image", __func__, NULL);

    if (gplotMakeOutput(gplot))
        return (PIX *)ERROR_PTR("plot output not made", __func__, NULL);
    return pixRead(gplot->outname);
}


/*!
 * \brief   gplotMakeOutput()
 *
 * \param[in]    gplot
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This uses gplot and the new arrays to add a plot
 *          to the output, by writing a new data file and appending
 *          the appropriate plot commands to the command file.
 *      (2) Along with gplotMakeOutputPix(), these are the only functions
 *          in this file that requires the gnuplot executable to
 *          actually generate the plot.
 *      (3) The command file name for unix is canonical (i.e., directory /tmp)
 *          but the temp filename paths in the command file must be correct.
 *      (4) The gnuplot program for Windows is wgnuplot.exe.
 * </pre>
 */
l_ok
gplotMakeOutput(GPLOT  *gplot)
{
char     buf[Bufsize];
char    *cmdname;

    if (!gplot)
        return ERROR_INT("gplot not defined", __func__, 1);

    if (!LeptDebugOK) {
        L_INFO("running gnuplot is disabled; "
               "use setLeptDebugOK(1) to enable\n", __func__);
        return 0;
    }

#ifdef OS_IOS /* iOS 11 does not support system() */
    return ERROR_INT("iOS 11 does not support system()", __func__, 0);
#endif /* OS_IOS */

    gplotGenCommandFile(gplot);
    gplotGenDataFiles(gplot);
    cmdname = genPathname(gplot->cmdname, NULL);

#ifndef _WIN32
    snprintf(buf, Bufsize, "gnuplot %s", cmdname);
#else
    snprintf(buf, Bufsize, "wgnuplot %s", cmdname);
#endif  /* _WIN32 */

    callSystemDebug(buf);  /* gnuplot || wgnuplot */
    LEPT_FREE(cmdname);
    return 0;
}


/*!
 * \brief   gplotGenCommandFile()
 *
 * \param[in]    gplot
 * \return  0 if OK, 1 on error
 */
l_ok
gplotGenCommandFile(GPLOT  *gplot)
{
char     buf[Bufsize];
char    *cmdstr, *plotlabel, *dataname;
l_int32  i, plotstyle, nplots;
FILE    *fp;

    if (!gplot)
        return ERROR_INT("gplot not defined", __func__, 1);

        /* Remove any previous command data */
    sarrayClear(gplot->cmddata);

        /* Generate command data instructions */
    if (gplot->title) {   /* set title */
        snprintf(buf, Bufsize, "set title '%s'", gplot->title);
        sarrayAddString(gplot->cmddata, buf, L_COPY);
    }
    if (gplot->xlabel) {   /* set xlabel */
        snprintf(buf, Bufsize, "set xlabel '%s'", gplot->xlabel);
        sarrayAddString(gplot->cmddata, buf, L_COPY);
    }
    if (gplot->ylabel) {   /* set ylabel */
        snprintf(buf, Bufsize, "set ylabel '%s'", gplot->ylabel);
        sarrayAddString(gplot->cmddata, buf, L_COPY);
    }

        /* Set terminal type and output */
    if (gplot->outformat == GPLOT_PNG) {
        snprintf(buf, Bufsize, "set terminal png; set output '%s'",
                 gplot->outname);
    } else if (gplot->outformat == GPLOT_PS) {
        snprintf(buf, Bufsize, "set terminal postscript; set output '%s'",
                 gplot->outname);
    } else if (gplot->outformat == GPLOT_EPS) {
        snprintf(buf, Bufsize, "set terminal postscript eps; set output '%s'",
                 gplot->outname);
    } else if (gplot->outformat == GPLOT_LATEX) {
        snprintf(buf, Bufsize, "set terminal latex; set output '%s'",
                 gplot->outname);
    } else if (gplot->outformat == GPLOT_PNM) {
        snprintf(buf, Bufsize, "set terminal pbm color; set output '%s'",
                 gplot->outname);
    }
    sarrayAddString(gplot->cmddata, buf, L_COPY);

    if (gplot->scaling == GPLOT_LOG_SCALE_X ||
        gplot->scaling == GPLOT_LOG_SCALE_X_Y) {
        snprintf(buf, Bufsize, "set logscale x");
        sarrayAddString(gplot->cmddata, buf, L_COPY);
    }
    if (gplot->scaling == GPLOT_LOG_SCALE_Y ||
        gplot->scaling == GPLOT_LOG_SCALE_X_Y) {
        snprintf(buf, Bufsize, "set logscale y");
        sarrayAddString(gplot->cmddata, buf, L_COPY);
    }

    nplots = sarrayGetCount(gplot->datanames);
    for (i = 0; i < nplots; i++) {
        plotlabel = sarrayGetString(gplot->plotlabels, i, L_NOCOPY);
        dataname = sarrayGetString(gplot->datanames, i, L_NOCOPY);
        numaGetIValue(gplot->plotstyles, i, &plotstyle);
        if (nplots == 1) {
            snprintf(buf, Bufsize, "plot '%s' title '%s' %s",
                     dataname, plotlabel, gplotstylenames[plotstyle]);
        } else {
            if (i == 0)
                snprintf(buf, Bufsize, "plot '%s' title '%s' %s, \\",
                     dataname, plotlabel, gplotstylenames[plotstyle]);
            else if (i < nplots - 1)
                snprintf(buf, Bufsize, " '%s' title '%s' %s, \\",
                     dataname, plotlabel, gplotstylenames[plotstyle]);
            else
                snprintf(buf, Bufsize, " '%s' title '%s' %s",
                     dataname, plotlabel, gplotstylenames[plotstyle]);
        }
        sarrayAddString(gplot->cmddata, buf, L_COPY);
    }

        /* Write command data to file */
    cmdstr = sarrayToString(gplot->cmddata, 1);
    if ((fp = fopenWriteStream(gplot->cmdname, "w")) == NULL) {
        L_ERROR("stream not opened for command: %s\n", __func__, cmdstr);
        LEPT_FREE(cmdstr);
        return 1;
    }
    fwrite(cmdstr, 1, strlen(cmdstr), fp);
    fclose(fp);
    LEPT_FREE(cmdstr);
    return 0;
}


/*!
 * \brief   gplotGenDataFiles()
 *
 * \param[in]    gplot
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The pathnames in the gplot command file are actual pathnames,
 *          which can be in temp directories.  Consequently, they must not be
 *          rewritten by calling fopenWriteStream(), and we use fopen().
 * </pre>
 */
l_ok
gplotGenDataFiles(GPLOT  *gplot)
{
char    *plotdata, *dataname;
l_int32  i, nplots;
FILE    *fp;

    if (!gplot)
        return ERROR_INT("gplot not defined", __func__, 1);

    nplots = sarrayGetCount(gplot->datanames);
    for (i = 0; i < nplots; i++) {
        plotdata = sarrayGetString(gplot->plotdata, i, L_NOCOPY);
        dataname = sarrayGetString(gplot->datanames, i, L_NOCOPY);
        if ((fp = fopen(dataname, "w")) == NULL)
            return ERROR_INT_1("datafile stream not opened",
                               dataname, __func__, 1);
        fwrite(plotdata, 1, strlen(plotdata), fp);
        fclose(fp);
    }

    return 0;
}


/*-----------------------------------------------------------------*
 *                        Quick one-line plots                     *
 *-----------------------------------------------------------------*/
/*!
 * \brief   gplotSimple1()
 *
 * \param[in]    na          numa; plot Y_VS_I
 * \param[in]    outformat   GPLOT_PNG, GPLOT_PS, GPLOT_EPS,
 *                           GPLOT_LATEX, GPLOT_PNM
 * \param[in]    outroot     root of output files
 * \param[in]    title       [optional], can be NULL
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This generates a line plot of a numa, where the array value
 *          is plotted vs the array index.  The plot is generated
 *          in the specified output format; the title  is optional.
 *      (2) When calling these simple plot functions more than once, use
 *          different %outroot to avoid overwriting the output files.
 * </pre>
 */
l_ok
gplotSimple1(NUMA        *na,
             l_int32      outformat,
             const char  *outroot,
             const char  *title)
{
GPLOT  *gplot;

    gplot = gplotSimpleXY1(NULL, na, GPLOT_LINES, outformat, outroot, title);
    if (!gplot)
        return ERROR_INT("failed to generate plot", __func__, 1);
    gplotDestroy(&gplot);
    return 0;
}


/*!
 * \brief   gplotSimple2()
 *
 * \param[in]    na1         numa; plot with Y_VS_I
 * \param[in]    na2         ditto
 * \param[in]    outformat   GPLOT_PNG, GPLOT_PS, GPLOT_EPS,
 *                           GPLOT_LATEX, GPLOT_PNM
 * \param[in]    outroot     root of output files
 * \param[in]    title       [optional]
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This generates a line plot of two numa, where the array values
 *          are each plotted vs the array index.  The plot is generated
 *          in the specified output format; the title  is optional.
 *      (2) When calling these simple plot functions more than once, use
 *          different %outroot to avoid overwriting the output files.
 * </pre>
 */
l_ok
gplotSimple2(NUMA        *na1,
             NUMA        *na2,
             l_int32      outformat,
             const char  *outroot,
             const char  *title)
{
GPLOT  *gplot;

    gplot = gplotSimpleXY2(NULL, na1, na2, GPLOT_LINES,
                           outformat, outroot, title);
    if (!gplot)
        return ERROR_INT("failed to generate plot", __func__, 1);
    gplotDestroy(&gplot);
    return 0;
}


/*!
 * \brief   gplotSimpleN()
 *
 * \param[in]    naa         numaa; plot Y_VS_I for each numa
 * \param[in]    outformat   GPLOT_PNG, GPLOT_PS, GPLOT_EPS,
 *                           GPLOT_LATEX, GPLOT_PNM
 * \param[in]    outroot     root of output files
 * \param[in]    title       [optional]
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This generates a line plot of all numas in a numaa (array of numa),
 *          where the array values are each plotted vs the array index.
 *          The plot is generated in the specified output format;
 *          the title  is optional.
 *      (2) When calling these simple plot functions more than once, use
 *          different %outroot to avoid overwriting the output files.
 * </pre>
 */
l_ok
gplotSimpleN(NUMAA       *naa,
             l_int32      outformat,
             const char  *outroot,
             const char  *title)
{
GPLOT  *gplot;

    gplot = gplotSimpleXYN(NULL, naa, GPLOT_LINES, outformat, outroot, title);
    if (!gplot)
        return ERROR_INT("failed to generate plot", __func__, 1);
    gplotDestroy(&gplot);
    return 0;
}


/*!
 * \brief   gplotSimplePix1()
 *
 * \param[in]    na          numa; plot Y_VS_I
 * \param[in]    title       [optional], can be NULL
 * \return  pix   of plot, or null on error
 *
 * <pre>
 * Notes:
 *      (1) This generates a line plot of a numa as a pix, where the array
 *          value is plotted vs the array index.  The title is optional.
 *      (2) The temporary plot file is a png; its name is generated internally
 *          and stored in gplot.
 * </pre>
 */
PIX *
gplotSimplePix1(NUMA        *na,
                const char  *title)
{
char            buf[64];
static l_atomic index;
GPLOT          *gplot;
PIX            *pix;

    if (!na)
        return (PIX *)ERROR_PTR("na not defined", __func__, NULL);

    lept_mkdir("lept/gplot/pix");
    snprintf(buf, sizeof(buf), "/tmp/lept/gplot/pix1.%d", index++);
    gplot = gplotSimpleXY1(NULL, na, GPLOT_LINES, GPLOT_PNG, buf, title);
    if (!gplot)
        return (PIX *)ERROR_PTR("failed to generate plot", __func__, NULL);
    pix = pixRead(gplot->outname);
    gplotDestroy(&gplot);
    if (!pix)
        return (PIX *)ERROR_PTR("failed to generate plot", __func__, NULL);
    return pix;
}


/*!
 * \brief   gplotSimplePix2()
 *
 * \param[in]    na1         numa; plot with Y_VS_I
 * \param[in]    na2         ditto
 * \param[in]    title       [optional], can be NULL
 * \return  pix   of plot, or null on error
 *
 * <pre>
 * Notes:
 *      (1) This generates a pix with line plots of two numa, where each of
 *          two arrays is plotted vs the array index.  the title is optional.
 *      (2) The temporary plot file is a png; its name is generated internally
 *          and stored in gplot.
 * </pre>
 */
PIX *
gplotSimplePix2(NUMA        *na1,
                NUMA        *na2,
                const char  *title)
{
char            buf[64];
static l_atomic index;
GPLOT          *gplot;
PIX            *pix;

    if (!na1 || !na2)
        return (PIX *)ERROR_PTR("both na1, na2 not defined", __func__, NULL);

    lept_mkdir("lept/gplot/pix");
    snprintf(buf, sizeof(buf), "/tmp/lept/gplot/pix2.%d", index++);
    gplot = gplotSimpleXY2(NULL, na1, na2, GPLOT_LINES, GPLOT_PNG, buf, title);
    if (!gplot)
        return (PIX *)ERROR_PTR("failed to generate plot", __func__, NULL);
    pix = pixRead(gplot->outname);
    gplotDestroy(&gplot);
    if (!pix)
        return (PIX *)ERROR_PTR("failed to generate plot", __func__, NULL);
    return pix;
}


/*!
 * \brief   gplotSimplePixN()
 *
 * \param[in]    naa         numaa; plot Y_VS_I for each numa
 * \param[in]    title       [optional], can be NULL
 * \return  pix   of plot, or null on error
 *
 * <pre>
 * Notes:
 *      (1) This generates a pix with an arbitrary number of line plots,
 *          each coming from a numa in %naa.  Each array value is plotted
 *          vs the array index.  The title is optional.
 *      (2) The temporary plot file is a png; its name is generated internally
 *          and stored in gplot.
 * </pre>
 */
PIX *
gplotSimplePixN(NUMAA       *naa,
                const char  *title)
{
char            buf[64];
static l_atomic index;
GPLOT          *gplot;
PIX            *pix;

    if (!naa)
        return (PIX *)ERROR_PTR("naa not defined", __func__, NULL);

    lept_mkdir("lept/gplot/pix");
    snprintf(buf, sizeof(buf), "/tmp/lept/gplot/pixN.%d", index++);
    gplot = gplotSimpleXYN(NULL, naa, GPLOT_LINES, GPLOT_PNG, buf, title);
    if (!gplot)
        return (PIX *)ERROR_PTR("failed to generate plot", __func__, NULL);
    pix = pixRead(gplot->outname);
    gplotDestroy(&gplot);
    if (!pix)
        return (PIX *)ERROR_PTR("failed to generate plot", __func__, NULL);
    return pix;
}


/*!
 * \brief   gplotSimpleXY1()
 *
 * \param[in]    nax         [optional]
 * \param[in]    nay         [required]
 * \param[in]    plotstyle   GPLOT_LINES, GPLOT_POINTS, GPLOT_IMPULSES,
 *                           GPLOT_LINESPOINTS, GPLOT_DOTS
 * \param[in]    outformat   GPLOT_PNG, GPLOT_PS, GPLOT_EPS,
 *                           GPLOT_LATEX, GPLOT_PNM
 * \param[in]    outroot     root of output files
 * \param[in]    title       [optional], can be NULL
 * \return  gplot   or null on error
 *
 * <pre>
 * Notes:
 *      (1) This generates a plot of a %nay vs %nax, generated in
 *          the specified output format.  The title is optional.
 *      (2) Use 0 for default plotstyle (lines).
 *      (3) %nax is optional.  If NULL, %nay is plotted against
 *          the array index.
 *      (4) When calling these simple plot functions more than once, use
 *          different %outroot to avoid overwriting the output files.
 *      (5) The returned gplot must be destroyed by the caller.
 * </pre>
 */
GPLOT *
gplotSimpleXY1(NUMA        *nax,
               NUMA        *nay,
               l_int32      plotstyle,
               l_int32      outformat,
               const char  *outroot,
               const char  *title)
{
GPLOT  *gplot;

    if (!nay)
        return (GPLOT *)ERROR_PTR("nay not defined", __func__, NULL);
    if (plotstyle < 0 || plotstyle >= NUM_GPLOT_STYLES)
        return (GPLOT *)ERROR_PTR("invalid plotstyle", __func__, NULL);
    if (outformat != GPLOT_PNG && outformat != GPLOT_PS &&
        outformat != GPLOT_EPS && outformat != GPLOT_LATEX &&
        outformat != GPLOT_PNM)
        return (GPLOT *)ERROR_PTR("invalid outformat", __func__, NULL);
    if (!outroot)
        return (GPLOT *)ERROR_PTR("outroot not specified", __func__, NULL);

    if ((gplot = gplotCreate(outroot, outformat, title, NULL, NULL)) == 0)
        return (GPLOT *)ERROR_PTR("gplot not made", __func__, NULL);
    gplotAddPlot(gplot, nax, nay, plotstyle, NULL);
    gplotMakeOutput(gplot);
    return gplot;
}


/*!
 * \brief   gplotSimpleXY2()
 *
 * \param[in]    nax          [optional], can be NULL
 * \param[in]    nay1
 * \param[in]    nay2
 * \param[in]    plotstyle    GPLOT_LINES, GPLOT_POINTS, GPLOT_IMPULSES,
 *                            GPLOT_LINESPOINTS, GPLOT_DOTS
 * \param[in]    outformat    GPLOT_PNG, GPLOT_PS, GPLOT_EPS,
 *                            GPLOT_LATEX, GPLOT_PNM
 * \param[in]    outroot      root of output files
 * \param[in]    title        [optional]
 * \return  gplot   or null on error
 *
 * <pre>
 * Notes:
 *      (1) This generates plots of %nay1 and %nay2 against %nax, generated
 *          in the specified output format.  The title is optional.
 *      (2) Use 0 for default plotstyle (lines).
 *      (3) %nax is optional.  If NULL, %nay1 and %nay2 are plotted
 *          against the array index.
 *      (4) When calling these simple plot functions more than once, use
 *          different %outroot to avoid overwriting the output files.
 *      (5) The returned gplot must be destroyed by the caller.
 * </pre>
 */
GPLOT *
gplotSimpleXY2(NUMA        *nax,
               NUMA        *nay1,
               NUMA        *nay2,
               l_int32      plotstyle,
               l_int32      outformat,
               const char  *outroot,
               const char  *title)
{
GPLOT  *gplot;

    if (!nay1 || !nay2)
        return (GPLOT *)ERROR_PTR("nay1 and nay2 not both defined",
                                  __func__, NULL);
    if (plotstyle < 0 || plotstyle >= NUM_GPLOT_STYLES)
        return (GPLOT *)ERROR_PTR("invalid plotstyle", __func__, NULL);
    if (outformat != GPLOT_PNG && outformat != GPLOT_PS &&
        outformat != GPLOT_EPS && outformat != GPLOT_LATEX &&
        outformat != GPLOT_PNM)
        return (GPLOT *)ERROR_PTR("invalid outformat", __func__, NULL);
    if (!outroot)
        return (GPLOT *)ERROR_PTR("outroot not specified", __func__, NULL);

    if ((gplot = gplotCreate(outroot, outformat, title, NULL, NULL)) == 0)
        return (GPLOT *)ERROR_PTR("gplot not made", __func__, NULL);
    gplotAddPlot(gplot, nax, nay1, plotstyle, NULL);
    gplotAddPlot(gplot, nax, nay2, plotstyle, NULL);
    gplotMakeOutput(gplot);
    return gplot;
}


/*!
 * \brief   gplotSimpleXYN()
 *
 * \param[in]    nax          [optional]; can be NULL
 * \param[in]    naay         numaa of arrays to plot against %nax
 * \param[in]    plotstyle    GPLOT_LINES, GPLOT_POINTS, GPLOT_IMPULSES,
 *                            GPLOT_LINESPOINTS, GPLOT_DOTS
 * \param[in]    outformat    GPLOT_PNG, GPLOT_PS, GPLOT_EPS,
 *                            GPLOT_LATEX, GPLOT_PNM
 * \param[in]    outroot      root of output files
 * \param[in]    title        [optional]
 * \return  gplot  or null on error
 *
 * <pre>
 * Notes:
 *      (1) This generates plots of each Numa in %naa against %nax,
 *          generated in the specified output format.  The title is optional.
 *      (2) Use 0 for default plotstyle (lines).
 *      (3) %nax is optional.  If NULL, each Numa array is plotted against
 *          the array index.
 *      (4) When calling these simple plot functions more than once, use
 *          different %outroot to avoid overwriting the output files.
 *      (5) The returned gplot must be destroyed by the caller.
 * </pre>
 */
GPLOT *
gplotSimpleXYN(NUMA        *nax,
               NUMAA       *naay,
               l_int32      plotstyle,
               l_int32      outformat,
               const char  *outroot,
               const char  *title)
{
l_int32  i, n;
GPLOT   *gplot;
NUMA    *nay;

    if (!naay)
        return (GPLOT *)ERROR_PTR("naay not defined", __func__, NULL);
    if ((n = numaaGetCount(naay)) == 0)
        return (GPLOT *)ERROR_PTR("no numa in array", __func__, NULL);
    if (plotstyle < 0 || plotstyle >= NUM_GPLOT_STYLES)
        return (GPLOT *)ERROR_PTR("invalid plotstyle", __func__, NULL);
    if (outformat != GPLOT_PNG && outformat != GPLOT_PS &&
        outformat != GPLOT_EPS && outformat != GPLOT_LATEX &&
        outformat != GPLOT_PNM)
        return (GPLOT *)ERROR_PTR("invalid outformat", __func__, NULL);
    if (!outroot)
        return (GPLOT *)ERROR_PTR("outroot not specified", __func__, NULL);

    if ((gplot = gplotCreate(outroot, outformat, title, NULL, NULL)) == 0)
        return (GPLOT *)ERROR_PTR("gplot not made", __func__, NULL);
    for (i = 0; i < n; i++) {
        nay = numaaGetNuma(naay, i, L_CLONE);
        gplotAddPlot(gplot, nax, nay, plotstyle, NULL);
        numaDestroy(&nay);
    }
    gplotMakeOutput(gplot);
    return gplot;
}


/*!
 * \brief   gplotGeneralPix1()
 *
 * \param[in]    na          data array
 * \param[in]    plotstyle   GPLOT_LINES, GPLOT_POINTS, GPLOT_IMPULSES,
 *                           GPLOT_LINESPOINTS, GPLOT_DOTS
 * \param[in]    rootname    root for all output files
 * \param[in]    title       [optional] overall title
 * \param[in]    xlabel      [optional] x axis label
 * \param[in]    ylabel      [optional] y axis label
 * \return  pix   of plot, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The 'title', 'xlabel' and 'ylabel' strings can have spaces,
 *          double quotes and backquotes, but not single quotes.
 * </pre>
 */
PIX *
gplotGeneralPix1(NUMA        *na,
                 l_int32      plotstyle,
                 const char  *rootname,
                 const char  *title,
                 const char  *xlabel,
                 const char  *ylabel)
{
GPLOT *gplot;
PIX   *pix;

    if (!na)
        return (PIX *)ERROR_PTR("na not defined", __func__, NULL);
    if (plotstyle < 0 || plotstyle >= NUM_GPLOT_STYLES)
        return (PIX *)ERROR_PTR("invalid plotstyle", __func__, NULL);
    if (!rootname)
        return (PIX *)ERROR_PTR("rootname not defined", __func__, NULL);

    gplot = gplotCreate(rootname, GPLOT_PNG, title, xlabel, ylabel);
    if (!gplot)
        return (PIX *)ERROR_PTR("gplot not made", __func__, NULL);
    gplotAddPlot(gplot, NULL, na, plotstyle, NULL);
    pix = gplotMakeOutputPix(gplot);
    gplotDestroy(&gplot);
    return pix;
}


/*!
 * \brief   gplotGeneralPix2()
 *
 * \param[in]    na1         x-axis data array
 * \param[in]    na2         y-axis data array
 * \param[in]    plotstyle   GPLOT_LINES, GPLOT_POINTS, GPLOT_IMPULSES,
 *                           GPLOT_LINESPOINTS, GPLOT_DOTS
 * \param[in]    rootname    root for all output files
 * \param[in]    title       [optional] overall title
 * \param[in]    xlabel      [optional] x axis label
 * \param[in]    ylabel      [optional] y axis label
 * \return  pix   of plot, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The 'title', 'xlabel' and 'ylabel' strings can have spaces,
 *          double quotes and backquotes, but not single quotes.
 * </pre>
 */
PIX *
gplotGeneralPix2(NUMA        *na1,
                 NUMA        *na2,
                 l_int32      plotstyle,
                 const char  *rootname,
                 const char  *title,
                 const char  *xlabel,
                 const char  *ylabel)
{
GPLOT *gplot;
PIX   *pix;

    if (!na1)
        return (PIX *)ERROR_PTR("na1 not defined", __func__, NULL);
    if (!na2)
        return (PIX *)ERROR_PTR("na2 not defined", __func__, NULL);
    if (plotstyle < 0 || plotstyle >= NUM_GPLOT_STYLES)
        return (PIX *)ERROR_PTR("invalid plotstyle", __func__, NULL);
    if (!rootname)
        return (PIX *)ERROR_PTR("rootname not defined", __func__, NULL);

    gplot = gplotCreate(rootname, GPLOT_PNG, title, xlabel, ylabel);
    if (!gplot)
        return (PIX *)ERROR_PTR("gplot not made", __func__, NULL);
    gplotAddPlot(gplot, na1, na2, plotstyle, NULL);
    pix = gplotMakeOutputPix(gplot);
    gplotDestroy(&gplot);
    return pix;
}


/*!
 * \brief   gplotGeneralPixN()
 *
 * \param[in]    nax         x-axis data array
 * \param[in]    naay        array of y-axis data arrays
 * \param[in]    plotstyle   GPLOT_LINES, GPLOT_POINTS, GPLOT_IMPULSES,
 *                           GPLOT_LINESPOINTS, GPLOT_DOTS
 * \param[in]    rootname    root for all output files
 * \param[in]    title       [optional] overall title
 * \param[in]    xlabel      [optional] x axis label
 * \param[in]    ylabel      [optional] y axis label
 * \return  pix   of plot, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The 'title', 'xlabel' and 'ylabel' strings can have spaces,
 *          double quotes and backquotes, but not single quotes.
 * </pre>
 */
PIX *
gplotGeneralPixN(NUMA        *nax,
                 NUMAA       *naay,
                 l_int32      plotstyle,
                 const char  *rootname,
                 const char  *title,
                 const char  *xlabel,
                 const char  *ylabel)
{
l_int32  i, n;
GPLOT   *gplot;
NUMA    *nay;
PIX     *pix;

    if (!nax)
        return (PIX *)ERROR_PTR("nax not defined", __func__, NULL);
    if (!naay)
        return (PIX *)ERROR_PTR("naay not defined", __func__, NULL);
    if ((n = numaaGetCount(naay)) == 0)
        return (PIX *)ERROR_PTR("no numa in array", __func__, NULL);
    if (plotstyle < 0 || plotstyle >= NUM_GPLOT_STYLES)
        return (PIX *)ERROR_PTR("invalid plotstyle", __func__, NULL);
    if (!rootname)
        return (PIX *)ERROR_PTR("rootname not defined", __func__, NULL);

    gplot = gplotCreate(rootname, GPLOT_PNG, title, xlabel, ylabel);
    if (!gplot)
        return (PIX *)ERROR_PTR("gplot not made", __func__, NULL);
    for (i = 0; i < n; i++) {
        nay = numaaGetNuma(naay, i, L_CLONE);
        gplotAddPlot(gplot, nax, nay, plotstyle, NULL);
        numaDestroy(&nay);
    }
    pix = gplotMakeOutputPix(gplot);
    gplotDestroy(&gplot);
    return pix;
}


/*-----------------------------------------------------------------*
 *                           Serialize for I/O                     *
 *-----------------------------------------------------------------*/
/*!
 * \brief   gplotRead()
 *
 * \param[in]    filename
 * \return  gplot, or NULL on error
 */
GPLOT *
gplotRead(const char  *filename)
{
char     buf[Bufsize];
char    *rootname, *title, *xlabel, *ylabel, *ignores;
l_int32  outformat, ret, version, ignore;
FILE    *fp;
GPLOT   *gplot;

    if (!filename)
        return (GPLOT *)ERROR_PTR("filename not defined", __func__, NULL);

    if ((fp = fopenReadStream(filename)) == NULL)
        return (GPLOT *)ERROR_PTR_1("stream not opened",
                                    filename, __func__, NULL);

    ret = fscanf(fp, "Gplot Version %d\n", &version);
    if (ret != 1) {
        fclose(fp);
        return (GPLOT *)ERROR_PTR_1("not a gplot file",
                                    filename, __func__, NULL);
    }
    if (version != GPLOT_VERSION_NUMBER) {
        fclose(fp);
        return (GPLOT *)ERROR_PTR_1("invalid gplot version",
                                    filename, __func__, NULL);
    }

    ignore = fscanf(fp, "Rootname: %511s\n", buf);  /* Bufsize - 1 */
    rootname = stringNew(buf);
    ignore = fscanf(fp, "Output format: %d\n", &outformat);
    ignores = fgets(buf, Bufsize, fp);   /* Title: ... */
    title = stringNew(buf + 7);
    title[strlen(title) - 1] = '\0';
    ignores = fgets(buf, Bufsize, fp);   /* X axis label: ... */
    xlabel = stringNew(buf + 14);
    xlabel[strlen(xlabel) - 1] = '\0';
    ignores = fgets(buf, Bufsize, fp);   /* Y axis label: ... */
    ylabel = stringNew(buf + 14);
    ylabel[strlen(ylabel) - 1] = '\0';

    gplot = gplotCreate(rootname, outformat, title, xlabel, ylabel);
    LEPT_FREE(rootname);
    LEPT_FREE(title);
    LEPT_FREE(xlabel);
    LEPT_FREE(ylabel);
    if (!gplot) {
        fclose(fp);
        return (GPLOT *)ERROR_PTR_1("gplot not made", filename, __func__, NULL);
    }
    sarrayDestroy(&gplot->cmddata);
    sarrayDestroy(&gplot->datanames);
    sarrayDestroy(&gplot->plotdata);
    sarrayDestroy(&gplot->plotlabels);
    numaDestroy(&gplot->plotstyles);

    ignore = fscanf(fp, "Commandfile name: %s\n", buf);  /* Bufsize - 1 */
    stringReplace(&gplot->cmdname, buf);
    ignore = fscanf(fp, "\nCommandfile data:");
    gplot->cmddata = sarrayReadStream(fp);
    ignore = fscanf(fp, "\nDatafile names:");
    gplot->datanames = sarrayReadStream(fp);
    ignore = fscanf(fp, "\nPlot data:");
    gplot->plotdata = sarrayReadStream(fp);
    ignore = fscanf(fp, "\nPlot titles:");
    gplot->plotlabels = sarrayReadStream(fp);
    ignore = fscanf(fp, "\nPlot styles:");
    gplot->plotstyles = numaReadStream(fp);

    ignore = fscanf(fp, "Number of plots: %d\n", &gplot->nplots);
    ignore = fscanf(fp, "Output file name: %s\n", buf);
    stringReplace(&gplot->outname, buf);
    ignore = fscanf(fp, "Axis scaling: %d\n", &gplot->scaling);

    fclose(fp);
    return gplot;
}


/*!
 * \brief   gplotWrite()
 *
 * \param[in]    filename
 * \param[in]    gplot
 * \return  0 if OK; 1 on error
 */
l_ok
gplotWrite(const char  *filename,
           GPLOT       *gplot)
{
FILE  *fp;

    if (!filename)
        return ERROR_INT("filename not defined", __func__, 1);
    if (!gplot)
        return ERROR_INT("gplot not defined", __func__, 1);

    if ((fp = fopenWriteStream(filename, "wb")) == NULL)
        return ERROR_INT_1("stream not opened", filename, __func__, 1);

    fprintf(fp, "Gplot Version %d\n", GPLOT_VERSION_NUMBER);
    fprintf(fp, "Rootname: %s\n", gplot->rootname);
    fprintf(fp, "Output format: %d\n", gplot->outformat);
    fprintf(fp, "Title: %s\n", gplot->title);
    fprintf(fp, "X axis label: %s\n", gplot->xlabel);
    fprintf(fp, "Y axis label: %s\n", gplot->ylabel);

    fprintf(fp, "Commandfile name: %s\n", gplot->cmdname);
    fprintf(fp, "\nCommandfile data:");
    sarrayWriteStream(fp, gplot->cmddata);
    fprintf(fp, "\nDatafile names:");
    sarrayWriteStream(fp, gplot->datanames);
    fprintf(fp, "\nPlot data:");
    sarrayWriteStream(fp, gplot->plotdata);
    fprintf(fp, "\nPlot titles:");
    sarrayWriteStream(fp, gplot->plotlabels);
    fprintf(fp, "\nPlot styles:");
    numaWriteStderr(gplot->plotstyles);

    fprintf(fp, "Number of plots: %d\n", gplot->nplots);
    fprintf(fp, "Output file name: %s\n", gplot->outname);
    fprintf(fp, "Axis scaling: %d\n", gplot->scaling);

    fclose(fp);
    return 0;
}
