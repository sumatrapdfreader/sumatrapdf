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
 * \file regutils.c
 * <pre>
 *
 *       Regression test utilities
 *           l_int32    regTestSetup()
 *           l_int32    regTestCleanup()
 *           l_int32    regTestCompareValues()
 *           l_int32    regTestCompareStrings()
 *           l_int32    regTestComparePix()
 *           l_int32    regTestCompareSimilarPix()
 *           l_int32    regTestCheckFile()
 *           l_int32    regTestCompareFiles()
 *           l_int32    regTestWritePixAndCheck()
 *           l_int32    regTestWriteDataAndCheck()
 *           char      *regTestGenLocalFilename()
 *
 *       Static function
 *           char      *getRootNameFromArgv0()
 *
 *  These functions are for testing and development.  They are not intended
 *  for use with programs that run in a production environment, such as a
 *  cloud service with unrestricted access.
 *
 *  See regutils.h for how to use this.  Here is a minimal setup:
 *
 *  main(int argc, char **argv) {
 *  ...
 *  L_REGPARAMS  *rp;
 *
 *      if (regTestSetup(argc, argv, &rp))
 *          return 1;
 *      ...
 *      regTestWritePixAndCheck(rp, pix, IFF_PNG);  // 0
 *      ...
 *      return regTestCleanup(rp);
 *  }
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"

extern l_int32 NumImageFileFormatExtensions;
extern const char *ImageFileFormatExtensions[];

static char *getRootNameFromArgv0(const char *argv0);


/*--------------------------------------------------------------------*
 *                      Regression test utilities                     *
 *--------------------------------------------------------------------*/
/*!
 * \brief   regTestSetup()
 *
 * \param[in]    argc    from invocation; can be either 1 or 2
 * \param[in]    argv    to regtest: %argv[1] is one of these:
 *                       "generate", "compare", "display"
 * \param[out]   prp     all regression params
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Call this function with the args to the reg test.  The first arg
 *          is the name of the reg test.  There are three cases:
 *          Case 1:
 *              There is either only one arg, or the second arg is "compare".
 *              This is the mode in which you run a regression test
 *              (or a set of them), looking for failures and logging
 *              the results to a file.  The output, which includes
 *              logging of all reg test failures plus a SUCCESS or
 *              FAILURE summary for each test, is appended to the file
 *              "/tmp/lept/reg_results.txt.  For this case, as in Case 2,
 *              the display field in rp is set to FALSE, preventing
 *              image display.
 *          Case 2:
 *              The second arg is "generate".  This will cause
 *              generation of new golden files for the reg test.
 *              The results of the reg test are not recorded, and
 *              the display field in rp is set to FALSE.
 *          Case 3:
 *              The second arg is "display".  The test will run and
 *              files will be written.  Comparisons with golden files
 *              will not be carried out, so the only notion of success
 *              or failure is with tests that do not involve golden files.
 *              The display field in rp is TRUE, and this is used by
 *              pixDisplayWithTitle().
 *      (2) See regutils.h for examples of usage.
 * </pre>
 */
l_ok
regTestSetup(l_int32        argc,
             char         **argv,
             L_REGPARAMS  **prp)
{
char         *testname, *vers;
char          errormsg[64];
L_REGPARAMS  *rp;

    if (argc != 1 && argc != 2) {
        snprintf(errormsg, sizeof(errormsg),
            "Syntax: %s [ [compare] | generate | display ]", argv[0]);
        return ERROR_INT(errormsg, __func__, 1);
    }

    if ((testname = getRootNameFromArgv0(argv[0])) == NULL)
        return ERROR_INT("invalid root", __func__, 1);

    setLeptDebugOK(1);  /* required for testing */

    rp = (L_REGPARAMS *)LEPT_CALLOC(1, sizeof(L_REGPARAMS));
    *prp = rp;
    rp->testname = testname;
    rp->index = -1;  /* increment before each test */

        /* Initialize to true.  A failure in any test is registered
         * as a failure of the regression test. */
    rp->success = TRUE;

        /* Make sure the lept/regout subdirectory exists */
    lept_mkdir("lept/regout");

        /* Only open a stream to a temp file for the 'compare' case */
    if (argc == 1 || !strcmp(argv[1], "compare")) {
        rp->mode = L_REG_COMPARE;
        rp->tempfile = stringNew("/tmp/lept/regout/regtest_output.txt");
        rp->fp = fopenWriteStream(rp->tempfile, "wb");
        if (rp->fp == NULL) {
            rp->success = FALSE;
            return ERROR_INT_1("stream not opened for tempfile",
                               rp->tempfile, __func__, 1);
        }
    } else if (!strcmp(argv[1], "generate")) {
        rp->mode = L_REG_GENERATE;
        lept_mkdir("lept/golden");
    } else if (!strcmp(argv[1], "display")) {
        rp->mode = L_REG_DISPLAY;
        rp->display = TRUE;
    } else {
        LEPT_FREE(rp);
        snprintf(errormsg, sizeof(errormsg),
            "Syntax: %s [ [generate] | compare | display ]", argv[0]);
        return ERROR_INT(errormsg, __func__, 1);
    }

        /* Print out test name and both the leptonica and
         * image library versions */
    lept_stderr("\n////////////////////////////////////////////////\n"
                "////////////////   %s_reg   ///////////////\n"
                "////////////////////////////////////////////////\n",
                rp->testname);
    vers = getLeptonicaVersion();
    lept_stderr("%s : ", vers);
    LEPT_FREE(vers);
    vers = getImagelibVersions();
    lept_stderr("%s\n", vers);
    LEPT_FREE(vers);

    rp->tstart = startTimerNested();
    return 0;
}


/*!
 * \brief   regTestCleanup()
 *
 * \param[in]    rp    regression test parameters
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This copies anything written to the temporary file to the
 *          output file /tmp/lept/reg_results.txt.
 * </pre>
 */
l_ok
regTestCleanup(L_REGPARAMS  *rp)
{
char     result[512];
char    *results_file;  /* success/failure output in 'compare' mode */
char    *text, *message;
l_int32  retval;
size_t   nbytes;

    if (!rp)
        return ERROR_INT("rp not defined", __func__, 1);

    lept_stderr("Time: %7.3f sec\n", stopTimerNested(rp->tstart));

        /* If generating golden files or running in display mode, release rp */
    if (!rp->fp) {
        LEPT_FREE(rp->testname);
        LEPT_FREE(rp->tempfile);
        LEPT_FREE(rp);
        return 0;
    }

        /* Compare mode: read back data from temp file */
    fclose(rp->fp);
    text = (char *)l_binaryRead(rp->tempfile, &nbytes);
    LEPT_FREE(rp->tempfile);
    if (!text) {
        rp->success = FALSE;
        LEPT_FREE(rp->testname);
        LEPT_FREE(rp);
        return ERROR_INT("text not returned", __func__, 1);
    }

        /* Prepare result message */
    if (rp->success)
        snprintf(result, sizeof(result), "SUCCESS: %s_reg\n", rp->testname);
    else
        snprintf(result, sizeof(result), "FAILURE: %s_reg\n", rp->testname);
    message = stringJoin(text, result);
    LEPT_FREE(text);
    results_file = stringNew("/tmp/lept/reg_results.txt");
    fileAppendString(results_file, message);
    retval = (rp->success) ? 0 : 1;
    LEPT_FREE(results_file);
    LEPT_FREE(message);

    LEPT_FREE(rp->testname);
    LEPT_FREE(rp);
    return retval;
}


/*!
 * \brief   regTestCompareValues()
 *
 * \param[in]    rp      regtest parameters
 * \param[in]    val1    typ. the golden value
 * \param[in]    val2    typ. the value computed
 * \param[in]    delta   allowed max absolute difference
 * \return  0 if OK, 1 on error
 *               Note: a failure in comparison is not an error
 */
l_ok
regTestCompareValues(L_REGPARAMS  *rp,
                     l_float32     val1,
                     l_float32     val2,
                     l_float32     delta)
{
l_float32  diff;

    if (!rp)
        return ERROR_INT("rp not defined", __func__, 1);

    rp->index++;
    diff = L_ABS(val2 - val1);

        /* Record on failure */
    if (diff > delta) {
        if (rp->fp) {
            fprintf(rp->fp,
                    "Failure in %s_reg: value comparison for index %d\n"
                    "difference = %f but allowed delta = %f\n",
                    rp->testname, rp->index, diff, delta);
        }
        lept_stderr("Failure in %s_reg: value comparison for index %d\n"
                    "difference = %f but allowed delta = %f\n",
                    rp->testname, rp->index, diff, delta);
        rp->success = FALSE;
    }
    return 0;
}


/*!
 * \brief   regTestCompareStrings()
 *
 * \param[in]    rp        regtest parameters
 * \param[in]    string1   typ. the expected string
 * \param[in]    bytes1    size of string1
 * \param[in]    string2   typ. the computed string
 * \param[in]    bytes2    size of string2
 * \return  0 if OK, 1 on error
 *               Note: a failure in comparison is not an error
 */
l_ok
regTestCompareStrings(L_REGPARAMS  *rp,
                      l_uint8      *string1,
                      size_t        bytes1,
                      l_uint8      *string2,
                      size_t        bytes2)
{
l_int32  same;
char     buf[256];

    if (!rp)
        return ERROR_INT("rp not defined", __func__, 1);

    rp->index++;
    l_binaryCompare(string1, bytes1, string2, bytes2, &same);

        /* Output on failure */
    if (!same) {
            /* Write the two strings to file */
        snprintf(buf, sizeof(buf), "/tmp/lept/regout/string1_%d_%zu",
                 rp->index, bytes1);
        l_binaryWrite(buf, "w", string1, bytes1);
        snprintf(buf, sizeof(buf), "/tmp/lept/regout/string2_%d_%zu",
                 rp->index, bytes2);
        l_binaryWrite(buf, "w", string2, bytes2);

            /* Report comparison failure */
        snprintf(buf, sizeof(buf), "/tmp/lept/regout/string*_%d_*", rp->index);
        if (rp->fp) {
            fprintf(rp->fp,
                    "Failure in %s_reg: string comp for index %d; "
                    "written to %s\n", rp->testname, rp->index, buf);
        }
        lept_stderr("Failure in %s_reg: string comp for index %d; "
                    "written to %s\n", rp->testname, rp->index, buf);
        rp->success = FALSE;
    }
    return 0;
}


/*!
 * \brief   regTestComparePix()
 *
 * \param[in]    rp            regtest parameters
 * \param[in]    pix1, pix2    to be tested for equality
 * \return  0 if OK, 1 on error
 *               Note: a failure in comparison is not an error
 *
 * <pre>
 * Notes:
 *      (1) This function compares two pix for equality.  On failure,
 *          this writes to stderr.
 * </pre>
 */
l_ok
regTestComparePix(L_REGPARAMS  *rp,
                  PIX          *pix1,
                  PIX          *pix2)
{
l_int32  same;

    if (!rp)
        return ERROR_INT("rp not defined", __func__, 1);
    if (!pix1 || !pix2) {
        rp->success = FALSE;
        return ERROR_INT("pix1 and pix2 not both defined", __func__, 1);
    }

    rp->index++;
    pixEqual(pix1, pix2, &same);

        /* Record on failure */
    if (!same) {
        if (rp->fp) {
            fprintf(rp->fp, "Failure in %s_reg: pix comparison for index %d\n",
                    rp->testname, rp->index);
        }
        lept_stderr("Failure in %s_reg: pix comparison for index %d\n",
                    rp->testname, rp->index);
        rp->success = FALSE;
    }
    return 0;
}


/*!
 * \brief   regTestCompareSimilarPix()
 *
 * \param[in]    rp           regtest parameters
 * \param[in]    pix1, pix2   to be tested for near equality
 * \param[in]    mindiff      minimum pixel difference to be counted; > 0
 * \param[in]    maxfract     maximum fraction of pixels allowed to have
 *                            diff greater than or equal to mindiff
 * \param[in]    printstats   use 1 to print normalized histogram to stderr
 * \return  0 if OK, 1 on error
 *               Note: a failure in similarity comparison is not an error
 *
 * <pre>
 * Notes:
 *      (1) This function compares two pix for near equality.  On failure,
 *          this writes to stderr.
 *      (2) The pix are similar if the fraction of non-conforming pixels
 *          does not exceed %maxfract.  Pixels are non-conforming if
 *          the difference in pixel values equals or exceeds %mindiff.
 *          Typical values might be %mindiff = 15 and %maxfract = 0.01.
 *      (3) The input images must have the same size and depth.  The
 *          pixels for comparison are typically subsampled from the images.
 *      (4) Normally, use %printstats = 0.  In debugging mode, to see
 *          the relation between %mindiff and the minimum value of
 *          %maxfract for success, set this to 1.
 * </pre>
 */
l_ok
regTestCompareSimilarPix(L_REGPARAMS  *rp,
                         PIX          *pix1,
                         PIX          *pix2,
                         l_int32       mindiff,
                         l_float32     maxfract,
                         l_int32       printstats)
{
l_int32  w, h, factor, similar;

    if (!rp)
        return ERROR_INT("rp not defined", __func__, 1);
    if (!pix1 || !pix2) {
        rp->success = FALSE;
        return ERROR_INT("pix1 and pix2 not both defined", __func__, 1);
    }

    rp->index++;
    pixGetDimensions(pix1, &w, &h, NULL);
    factor = L_MAX(w, h) / 400;
    factor = L_MAX(1, L_MIN(factor, 4));   /* between 1 and 4 */
    pixTestForSimilarity(pix1, pix2, factor, mindiff, maxfract, 0.0,
                         &similar, printstats);

        /* Record on failure */
    if (!similar) {
        if (rp->fp) {
            fprintf(rp->fp,
                    "Failure in %s_reg: pix similarity comp for index %d\n",
                    rp->testname, rp->index);
        }
        lept_stderr("Failure in %s_reg: pix similarity comp for index %d\n",
                    rp->testname, rp->index);
        rp->success = FALSE;
    }
    return 0;
}


/*!
 * \brief   regTestCheckFile()
 *
 * \param[in]    rp         regtest parameters
 * \param[in]    localname  name of output file from reg test
 * \return  0 if OK, 1 on error
 *               Note: a failure in comparison is not an error
 *
 * <pre>
 * Notes:
 *      (1) This function does one of three things, depending on the mode:
 *           * "generate": makes a "golden" file as a copy of %localname.
 *           * "compare": compares %localname contents with the golden file
 *           * "display": this does nothing
 *      (2) The canonical format of the golden filenames is:
 *            /tmp/lept/golden/[root of main name]_golden.[index].
 *                                                       [ext of localname]
 *          e.g.,
 *             /tmp/lept/golden/maze_golden.0.png
 *      (3) The local file can be made in any subdirectory of /tmp/lept,
 *          including /tmp/lept/regout/.
 *      (4) It is important to add an extension to the local name, such as
 *             /tmp/lept/maze/file1.png    (extension ".png")
 *          because the extension is added to the name of the golden file.
 * </pre>
 */
l_ok
regTestCheckFile(L_REGPARAMS  *rp,
                 const char   *localname)
{
char    *ext;
char     namebuf[256];
l_int32  ret, same, format;
PIX     *pix1, *pix2;

    if (!rp)
        return ERROR_INT("rp not defined", __func__, 1);
    if (!localname) {
        rp->success = FALSE;
        return ERROR_INT("local name not defined", __func__, 1);
    }
    if (rp->mode != L_REG_GENERATE && rp->mode != L_REG_COMPARE &&
        rp->mode != L_REG_DISPLAY) {
        rp->success = FALSE;
        return ERROR_INT("invalid mode", __func__, 1);
    }
    rp->index++;

        /* If display mode, no generation and no testing */
    if (rp->mode == L_REG_DISPLAY) return 0;

        /* Generate the golden file name; used in 'generate' and 'compare' */
    splitPathAtExtension(localname, NULL, &ext);
    snprintf(namebuf, sizeof(namebuf), "/tmp/lept/golden/%s_golden.%02d%s",
             rp->testname, rp->index, ext);
    LEPT_FREE(ext);

        /* Generate mode.  No testing. */
    if (rp->mode == L_REG_GENERATE) {
            /* Save the file as a golden file */
        ret = fileCopy(localname, namebuf);
#if 0       /* Enable for details on writing of golden files */
        if (!ret) {
            char *local = genPathname(localname, NULL);
            char *golden = genPathname(namebuf, NULL);
            L_INFO("Copy: %s to %s\n", __func__, local, golden);
            LEPT_FREE(local);
            LEPT_FREE(golden);
        }
#endif
        return ret;
    }

        /* Compare mode: test and record on failure.  This can be used
         * for all image formats, as well as for all files of serialized
         * data, such as boxa, pta, etc.  In all cases except for
         * GIF compressed images, we compare the files to see if they
         * are identical.  GIF doesn't support RGB images; to write
         * a 32 bpp RGB image in GIF, we do a lossy quantization to
         * 256 colors, so the cycle read-RGB/write-GIF is not idempotent.
         * And although the read/write cycle for GIF images with bpp <= 8
         * is idempotent in the image pixels, it is not idempotent in the
         * actual file bytes; tests comparing file bytes before and after
         * a GIF read/write cycle will fail.  So for GIF we uncompress
         * the two images and compare the actual pixels.  PNG is both
         * lossless and idempotent in file bytes on read/write, so it is
         * not necessary to compare pixels.  (Comparing pixels requires
         * decompression, and thus would increase the regression test
         * time.  JPEG is lossy and not idempotent in the image pixels,
         * so no tests are constructed that would require it. */
    findFileFormat(localname, &format);
    if (format == IFF_GIF) {
        same = 0;
        pix1 = pixRead(localname);
        pix2 = pixRead(namebuf);
        pixEqual(pix1, pix2, &same);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
    } else {
        filesAreIdentical(localname, namebuf, &same);
    }
    if (!same) {
        fprintf(rp->fp, "Failure in %s_reg, index %d: comparing %s with %s\n",
                rp->testname, rp->index, localname, namebuf);
        lept_stderr("Failure in %s_reg, index %d: comparing %s with %s\n",
                    rp->testname, rp->index, localname, namebuf);
        rp->success = FALSE;
    }

    return 0;
}


/*!
 * \brief   regTestCompareFiles()
 *
 * \param[in]    rp        regtest parameters
 * \param[in]    index1    of one output file from reg test
 * \param[in]    index2    of another output file from reg test
 * \return  0 if OK, 1 on error
 *               Note: a failure in comparison is not an error
 *
 * <pre>
 * Notes:
 *      (1) This only does something in "compare" mode.
 *      (2) The canonical format of the golden filenames is:
 *            /tmp/lept/golden/[root of main name]_golden.[index].
 *                                                      [ext of localname]
 *          e.g.,
 *            /tmp/lept/golden/maze_golden.0.png
 * </pre>
 */
l_ok
regTestCompareFiles(L_REGPARAMS  *rp,
                    l_int32       index1,
                    l_int32       index2)
{
char    *name1, *name2;
char     namebuf[256];
l_int32  same;
SARRAY  *sa;

    if (!rp)
        return ERROR_INT("rp not defined", __func__, 1);
    if (index1 < 0 || index2 < 0) {
        rp->success = FALSE;
        return ERROR_INT("index1 and/or index2 is negative", __func__, 1);
    }
    if (index1 == index2) {
        rp->success = FALSE;
        return ERROR_INT("index1 must differ from index2", __func__, 1);
    }

    rp->index++;
    if (rp->mode != L_REG_COMPARE) return 0;

        /* Generate the golden file names */
    snprintf(namebuf, sizeof(namebuf), "%s_golden.%02d", rp->testname, index1);
    sa = getSortedPathnamesInDirectory("/tmp/lept/golden", namebuf, 0, 0);
    if (sarrayGetCount(sa) != 1) {
        sarrayDestroy(&sa);
        rp->success = FALSE;
        L_ERROR("golden file %s not found\n", __func__, namebuf);
        return 1;
    }
    name1 = sarrayGetString(sa, 0, L_COPY);
    sarrayDestroy(&sa);

    snprintf(namebuf, sizeof(namebuf), "%s_golden.%02d", rp->testname, index2);
    sa = getSortedPathnamesInDirectory("/tmp/lept/golden", namebuf, 0, 0);
    if (sarrayGetCount(sa) != 1) {
        sarrayDestroy(&sa);
        rp->success = FALSE;
        LEPT_FREE(name1);
        L_ERROR("golden file %s not found\n", __func__, namebuf);
        return 1;
    }
    name2 = sarrayGetString(sa, 0, L_COPY);
    sarrayDestroy(&sa);

        /* Test and record on failure */
    filesAreIdentical(name1, name2, &same);
    if (!same) {
        fprintf(rp->fp,
                "Failure in %s_reg, index %d: comparing %s with %s\n",
                rp->testname, rp->index, name1, name2);
        lept_stderr("Failure in %s_reg, index %d: comparing %s with %s\n",
                    rp->testname, rp->index, name1, name2);
        rp->success = FALSE;
    }

    LEPT_FREE(name1);
    LEPT_FREE(name2);
    return 0;
}


/*!
 * \brief   regTestWritePixAndCheck()
 *
 * \param[in]    rp       regtest parameters
 * \param[in]    pix      to be written
 * \param[in]    format   of output pix
 * \return  0 if OK, 1 on error
 *               Note: a failure in comparison is not an error
 *
 * <pre>
 * Notes:
 *      (1) This function makes it easy to write the pix in a numbered
 *          sequence of files, and either to:
 *             (a) write the golden file ("generate" arg to regression test)
 *             (b) make a local file and "compare" with the golden file
 *             (c) make a local file and "display" the results
 *      (2) The canonical format of the local filename is:
 *            /tmp/lept/regout/[root of main name].[count].[format extension]
 *          e.g., for scale_reg,
 *            /tmp/lept/regout/scale.0.png
 *          The golden file name mirrors this in the usual way.
 *      (3) The check is done between the written files, which requires
 *          the files to be identical. The exception is for GIF, which
 *          only requires that all pixels in the decoded pix are identical.
 * </pre>
 */
l_ok
regTestWritePixAndCheck(L_REGPARAMS  *rp,
                        PIX          *pix,
                        l_int32       format)
{
char  namebuf[256];

    if (!rp)
        return ERROR_INT("rp not defined", __func__, 1);
    if (!pix) {
        rp->success = FALSE;
        return ERROR_INT("pix not defined", __func__, 1);
    }
    if (format < 0 || format >= NumImageFileFormatExtensions) {
        rp->success = FALSE;
        return ERROR_INT("invalid format", __func__, 1);
    }

        /* Use bmp format for testing if library for requested
         * format for jpeg, png or tiff is not available */
    changeFormatForMissingLib(&format);

        /* Generate the local file name */
    snprintf(namebuf, sizeof(namebuf), "/tmp/lept/regout/%s.%02d.%s",
             rp->testname, rp->index + 1, ImageFileFormatExtensions[format]);

        /* Write the local file */
    if (pixGetDepth(pix) < 8)
        pixSetPadBits(pix, 0);
    pixWrite(namebuf, pix, format);

        /* Either write the golden file ("generate") or check the
           local file against an existing golden file ("compare") */
    regTestCheckFile(rp, namebuf);

    return 0;
}


/*!
 * \brief   regTestWriteDataAndCheck()
 *
 * \param[in]    rp      regtest parameters
 * \param[in]    data    to be written
 * \param[in]    nbytes  of data to be written
 * \param[in]    ext     filename extension (e.g.: "ba", "pta")
 * \return  0 if OK, 1 on error
 *               Note: a failure in comparison is not an error
 *
 * <pre>
 * Notes:
 *      (1) This function makes it easy to write data in a numbered
 *          sequence of files, and either to:
 *             (a) write the golden file ("generate" arg to regression test)
 *             (b) make a local file and "compare" with the golden file
 *             (c) make a local file and "display" the results
 *      (2) The canonical format of the local filename is:
 *            /tmp/lept/regout/[root of main name].[count].[ext]
 *          e.g., for the first boxaa in quadtree_reg,
 *            /tmp/lept/regout/quadtree.0.baa
 *          The golden file name mirrors this in the usual way.
 *      (3) The data can be anything.  It is most useful for serialized
 *          output of data, such as boxa, pta, etc.
 *      (4) The file extension is arbitrary.  It is included simply
 *          to make the content type obvious when examining written files.
 *      (5) The check is done between the written files, which requires
 *          the files to be identical.
 * </pre>
 */
l_ok
regTestWriteDataAndCheck(L_REGPARAMS  *rp,
                         void         *data,
                         size_t        nbytes,
                         const char   *ext)
{
char  namebuf[256];

    if (!rp)
        return ERROR_INT("rp not defined", __func__, 1);
    if (!data || nbytes == 0) {
        rp->success = FALSE;
        return ERROR_INT("data not defined or size == 0", __func__, 1);
    }

        /* Generate the local file name */
    snprintf(namebuf, sizeof(namebuf), "/tmp/lept/regout/%s.%02d.%s",
             rp->testname, rp->index + 1, ext);

        /* Write the local file */
    l_binaryWrite(namebuf, "w", data, nbytes);

        /* Either write the golden file ("generate") or check the
           local file against an existing golden file ("compare") */
    regTestCheckFile(rp, namebuf);
    return 0;
}


/*!
 * \brief   regTestGenLocalFilename()
 *
 * \param[in]       rp      regtest parameters
 * \param[in]       index   use -1 for current index
 * \param[in]       format  of image; e.g., IFF_PNG
 * \return  filename if OK, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is used to get the name of a file in the regout
 *          subdirectory, that has been made and is used to test against
 *          the golden file.  You can either specify a particular index
 *          value, or with %index == -1, this returns the most recently
 *          written file.  The latter case lets you read a pix from a
 *          file that has just been written with regTestWritePixAndCheck(),
 *          which is useful for testing formatted read/write functions.
 *
 * </pre>
 */
char *
regTestGenLocalFilename(L_REGPARAMS  *rp,
                        l_int32       index,
                        l_int32       format)
{
char     buf[64];
l_int32  ind;

    if (!rp)
        return (char *)ERROR_PTR("rp not defined", __func__, NULL);

    ind = (index >= 0) ? index : rp->index;
    snprintf(buf, sizeof(buf), "/tmp/lept/regout/%s.%02d.%s",
             rp->testname, ind, ImageFileFormatExtensions[format]);
    return stringNew(buf);
}


/*!
 * \brief   getRootNameFromArgv0()
 *
 * \param[in]    argv0
 * \return  root name without the '_reg', or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) For example, from psioseg_reg, we want to extract
 *          just 'psioseg' as the root.
 *      (2) In unix with autotools, the executable is not X,
 *          but ./.libs/lt-X.   So in addition to stripping out the
 *          last 4 characters of the tail, we have to check for
 *          the '-' and strip out the "lt-" prefix if we find it.
 * </pre>
 */
static char *
getRootNameFromArgv0(const char  *argv0)
{
l_int32  len;
char    *root;

    splitPathAtDirectory(argv0, NULL, &root);
    if ((len = strlen(root)) <= 4) {
        LEPT_FREE(root);
        return (char *)ERROR_PTR("invalid argv0; too small", __func__, NULL);
    }

#ifndef _WIN32
    {
        char    *newroot;
        l_int32  loc;
        if (stringFindSubstr(root, "-", &loc)) {
            newroot = stringNew(root + loc + 1);  /* strip out "lt-" */
            LEPT_FREE(root);
            root = newroot;
            len = strlen(root);
        }
        len -= 4;  /* remove the "_reg" suffix */
    }
#else
    if (strstr(root, ".exe") != NULL)
        len -= 4;
    if (strstr(root, "_reg") == root + len - 4)
        len -= 4;
#endif  /* ! _WIN32 */

    root[len] = '\0';  /* terminate */
    return root;
}
