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
 * \file fmorphauto.c
 * <pre>
 *
 *    Main function calls:
 *       l_int32             fmorphautogen()
 *       l_int32             fmorphautogen1()
 *       l_int32             fmorphautogen2()
 *
 *    Static helpers:
 *       static SARRAY      *sarrayMakeWplsCode()
 *       static SARRAY      *sarrayMakeInnerLoopDWACode()
 *       static char        *makeBarrelshiftString()
 *
 *
 *    This automatically generates dwa code for erosion and dilation.
 *    Here's a road map for how it all works.
 *
 *    (1) You generate an array (a SELA) of structuring elements (SELs).
 *        This can be done in several ways, including
 *           (a) calling the function selaAddBasic() for
 *               pre-compiled SELs
 *           (b) generating the SELA in code in line
 *           (c) reading in a SELA from file, using selaRead() or
 *               various other formats.
 *
 *    (2) You call fmorphautogen() on this SELA.  This in turn calls
 *        fmorphautogen1() and fmorphautogen2(), which use the text
 *        files morphtemplate1.txt and morphtemplate2.txt, respectively,
 *        for building up the source code.  See the file
 *        prog/fmorphautogen.c for an example of how this is done.
 *        The output is written to files named fmorphgen.*.c
 *        and fmorphgenlow.*.c, where "*" is an integer that you
 *        input to this function.  That integer labels both
 *        the output files, as well as all the functions that
 *        are generated.  That way, using different integers,
 *        you can invoke fmorphautogen() any number of times
 *        to get functions that all have different names so that
 *        they can be linked into one program.
 *
 *    (3) You copy the generated source files back to your src
 *        directory for compilation.  Put their names in the
 *        Makefile, regenerate the allheaders.h prototype file,
 *        and recompile the library.  Look at the Makefile to see how
 *        morphgen.1.c and fmorphgenlow.1.c are included.  These files
 *        provide the single high-level interface for erosion, dilation,
 *        opening and closing, and the lower-level functions to
 *        do the actual work, for all 58 SELs in the SEL array.
 *
 *    (4) In an application, you now use this interface.  Again
 *        for the example files in the library, using integer "1":
 *
 *            PIX   *pixMorphDwa_1(PIX *pixd, PIX, *pixs,
 *                                 l_int32 operation, char *selname);
 *
 *        where the operation is one of {L_MORPH_DILATE, L_MORPH_ERODE.
 *        L_MORPH_OPEN, L_MORPH_CLOSE}, and the selname is one
 *        of the set that were defined as the name field of sels.
 *        This set is listed at the beginning of the file fmorphgen.1.c.
 *
 *        N.B. Although pixFMorphopGen_1() is global, you must NOT
 *        use it, because it assumes that 32 or 64 border pixels
 *        have been added to each side, and it will crash without those
 *        added pixels.
 *
 *        For examples of use, see the file prog/binmorph_reg1.c, which
 *        verifies the consistency of the various implementations by
 *        comparing the dwa result with that of full-image rasterops.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"

#define   OUTROOT         "fmorphgen"
#define   TEMPLATE1       "morphtemplate1.txt"
#define   TEMPLATE2       "morphtemplate2.txt"

#define   PROTOARGS   "(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);"

#define L_BUF_SIZE 512

static char * makeBarrelshiftString(l_int32 delx, l_int32 dely);
static SARRAY * sarrayMakeInnerLoopDWACode(SEL *sel, l_int32 index);
static SARRAY * sarrayMakeWplsCode(SEL *sel);

static char wpldecls[][53] = {
            "l_int32             wpls2;",
            "l_int32             wpls2, wpls3;",
            "l_int32             wpls2, wpls3, wpls4;",
            "l_int32             wpls5;",
            "l_int32             wpls5, wpls6;",
            "l_int32             wpls5, wpls6, wpls7;",
            "l_int32             wpls5, wpls6, wpls7, wpls8;",
            "l_int32             wpls9;",
            "l_int32             wpls9, wpls10;",
            "l_int32             wpls9, wpls10, wpls11;",
            "l_int32             wpls9, wpls10, wpls11, wpls12;",
            "l_int32             wpls13;",
            "l_int32             wpls13, wpls14;",
            "l_int32             wpls13, wpls14, wpls15;",
            "l_int32             wpls13, wpls14, wpls15, wpls16;",
            "l_int32             wpls17;",
            "l_int32             wpls17, wpls18;",
            "l_int32             wpls17, wpls18, wpls19;",
            "l_int32             wpls17, wpls18, wpls19, wpls20;",
            "l_int32             wpls21;",
            "l_int32             wpls21, wpls22;",
            "l_int32             wpls21, wpls22, wpls23;",
            "l_int32             wpls21, wpls22, wpls23, wpls24;",
            "l_int32             wpls25;",
            "l_int32             wpls25, wpls26;",
            "l_int32             wpls25, wpls26, wpls27;",
            "l_int32             wpls25, wpls26, wpls27, wpls28;",
            "l_int32             wpls29;",
            "l_int32             wpls29, wpls30;",
            "l_int32             wpls29, wpls30, wpls31;"};

static char wplgendecls[][30] = {
            "l_int32             wpls2;",
            "l_int32             wpls3;",
            "l_int32             wpls4;",
            "l_int32             wpls5;",
            "l_int32             wpls6;",
            "l_int32             wpls7;",
            "l_int32             wpls8;",
            "l_int32             wpls9;",
            "l_int32             wpls10;",
            "l_int32             wpls11;",
            "l_int32             wpls12;",
            "l_int32             wpls13;",
            "l_int32             wpls14;",
            "l_int32             wpls15;",
            "l_int32             wpls16;",
            "l_int32             wpls17;",
            "l_int32             wpls18;",
            "l_int32             wpls19;",
            "l_int32             wpls20;",
            "l_int32             wpls21;",
            "l_int32             wpls22;",
            "l_int32             wpls23;",
            "l_int32             wpls24;",
            "l_int32             wpls25;",
            "l_int32             wpls26;",
            "l_int32             wpls27;",
            "l_int32             wpls28;",
            "l_int32             wpls29;",
            "l_int32             wpls30;",
            "l_int32             wpls31;"};

static char wpldefs[][25] = {
            "    wpls2 = 2 * wpls;",
            "    wpls3 = 3 * wpls;",
            "    wpls4 = 4 * wpls;",
            "    wpls5 = 5 * wpls;",
            "    wpls6 = 6 * wpls;",
            "    wpls7 = 7 * wpls;",
            "    wpls8 = 8 * wpls;",
            "    wpls9 = 9 * wpls;",
            "    wpls10 = 10 * wpls;",
            "    wpls11 = 11 * wpls;",
            "    wpls12 = 12 * wpls;",
            "    wpls13 = 13 * wpls;",
            "    wpls14 = 14 * wpls;",
            "    wpls15 = 15 * wpls;",
            "    wpls16 = 16 * wpls;",
            "    wpls17 = 17 * wpls;",
            "    wpls18 = 18 * wpls;",
            "    wpls19 = 19 * wpls;",
            "    wpls20 = 20 * wpls;",
            "    wpls21 = 21 * wpls;",
            "    wpls22 = 22 * wpls;",
            "    wpls23 = 23 * wpls;",
            "    wpls24 = 24 * wpls;",
            "    wpls25 = 25 * wpls;",
            "    wpls26 = 26 * wpls;",
            "    wpls27 = 27 * wpls;",
            "    wpls28 = 28 * wpls;",
            "    wpls29 = 29 * wpls;",
            "    wpls30 = 30 * wpls;",
            "    wpls31 = 31 * wpls;"};

static char wplstrp[][10] = {"+ wpls", "+ wpls2", "+ wpls3", "+ wpls4",
                             "+ wpls5", "+ wpls6", "+ wpls7", "+ wpls8",
                             "+ wpls9", "+ wpls10", "+ wpls11", "+ wpls12",
                             "+ wpls13", "+ wpls14", "+ wpls15", "+ wpls16",
                             "+ wpls17", "+ wpls18", "+ wpls19", "+ wpls20",
                             "+ wpls21", "+ wpls22", "+ wpls23", "+ wpls24",
                             "+ wpls25", "+ wpls26", "+ wpls27", "+ wpls28",
                             "+ wpls29", "+ wpls30", "+ wpls31"};

static char wplstrm[][10] = {"- wpls", "- wpls2", "- wpls3", "- wpls4",
                             "- wpls5", "- wpls6", "- wpls7", "- wpls8",
                             "- wpls9", "- wpls10", "- wpls11", "- wpls12",
                             "- wpls13", "- wpls14", "- wpls15", "- wpls16",
                             "- wpls17", "- wpls18", "- wpls19", "- wpls20",
                             "- wpls21", "- wpls22", "- wpls23", "- wpls24",
                             "- wpls25", "- wpls26", "- wpls27", "- wpls28",
                             "- wpls29", "- wpls30", "- wpls31"};


/*!
 * \brief   fmorphautogen()
 *
 * \param[in]    sela
 * \param[in]    fileindex
 * \param[in]    filename [optional]; can be null
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This function generates all the code for implementing
 *          dwa morphological operations using all the sels in the sela.
 *      (2) See fmorphautogen1() and fmorphautogen2() for details.
 * </pre>
 */
l_ok
fmorphautogen(SELA        *sela,
              l_int32      fileindex,
              const char  *filename)
{
l_int32  ret1, ret2;

    if (!sela)
        return ERROR_INT("sela not defined", __func__, 1);
    ret1 = fmorphautogen1(sela, fileindex, filename);
    ret2 = fmorphautogen2(sela, fileindex, filename);
    if (ret1 || ret2)
        return ERROR_INT("code generation problem", __func__, 1);
    return 0;
}


/*!
 * \brief   fmorphautogen1()
 *
 * \param[in]    sela
 * \param[in]    fileindex
 * \param[in]    filename [optional]; can be null
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This function uses morphtemplate1.txt to create a
 *          top-level file that contains two functions.  These
 *          functions will carry out dilation, erosion,
 *          opening or closing for any of the sels in the input sela.
 *      (2) The fileindex parameter is inserted into the output
 *          filename, as described below.
 *      (3) If filename == NULL, the output file is fmorphgen.[n].c,
 *          where [n] is equal to the %fileindex parameter.
 *      (4) If filename != NULL, the output file is [%filename].[n].c.
 * </pre>
 */
l_ok
fmorphautogen1(SELA        *sela,
               l_int32      fileindex,
               const char  *filename)
{
char    *filestr;
char    *str_proto1, *str_proto2, *str_proto3;
char    *str_doc1, *str_doc2, *str_doc3, *str_doc4;
char    *str_def1, *str_def2, *str_proc1, *str_proc2;
char    *str_dwa1, *str_low_dt, *str_low_ds, *str_low_ts;
char    *str_low_tsp1, *str_low_dtp1;
char     bigbuf[L_BUF_SIZE];
l_int32  i, nsels, nbytes, actstart, end, newstart;
size_t   size;
SARRAY  *sa1, *sa2, *sa3;

    if (!sela)
        return ERROR_INT("sela not defined", __func__, 1);
    if (fileindex < 0)
        fileindex = 0;
    if ((nsels = selaGetCount(sela)) == 0)
        return ERROR_INT("no sels in sela", __func__, 1);

        /* Make array of textlines from morphtemplate1.txt */
    if ((filestr = (char *)l_binaryRead(TEMPLATE1, &size)) == NULL)
        return ERROR_INT("filestr not made", __func__, 1);
    sa2 = sarrayCreateLinesFromString(filestr, 1);
    LEPT_FREE(filestr);
    if (!sa2)
        return ERROR_INT("sa2 not made", __func__, 1);

        /* Make array of sel names */
    sa1 = selaGetSelnames(sela);

        /* Make strings containing function call names */
    sprintf(bigbuf, "PIX *pixMorphDwa_%d(PIX *pixd, PIX *pixs, "
                    "l_int32 operation, char *selname);", fileindex);
    str_proto1 = stringNew(bigbuf);
    sprintf(bigbuf, "PIX *pixFMorphopGen_%d(PIX *pixd, PIX *pixs, "
                    "l_int32 operation, char *selname);", fileindex);
    str_proto2 = stringNew(bigbuf);
    sprintf(bigbuf, "l_int32 fmorphopgen_low_%d(l_uint32 *datad, l_int32 w,\n"
        "                          l_int32 h, l_int32 wpld,\n"
        "                          l_uint32 *datas, l_int32 wpls,\n"
        "                          l_int32 index);", fileindex);
    str_proto3 = stringNew(bigbuf);
    sprintf(bigbuf, " *             PIX     *pixMorphDwa_%d()", fileindex);
    str_doc1 = stringNew(bigbuf);
    sprintf(bigbuf, " *             PIX     *pixFMorphopGen_%d()", fileindex);
    str_doc2 = stringNew(bigbuf);
    sprintf(bigbuf, " * \\brief   pixMorphDwa_%d()", fileindex);
    str_doc3 = stringNew(bigbuf);
    sprintf(bigbuf, " * \\brief   pixFMorphopGen_%d()", fileindex);
    str_doc4 = stringNew(bigbuf);
    sprintf(bigbuf, "pixMorphDwa_%d(PIX     *pixd,", fileindex);
    str_def1 = stringNew(bigbuf);
    sprintf(bigbuf, "pixFMorphopGen_%d(PIX     *pixd,", fileindex);
    str_def2 = stringNew(bigbuf);
    sprintf(bigbuf, "    PROCNAME(\"pixMorphDwa_%d\");", fileindex);
    str_proc1 = stringNew(bigbuf);
    sprintf(bigbuf, "    PROCNAME(\"pixFMorphopGen_%d\");", fileindex);
    str_proc2 = stringNew(bigbuf);
    sprintf(bigbuf,
            "    pixt2 = pixFMorphopGen_%d(NULL, pixt1, operation, selname);",
            fileindex);
    str_dwa1 = stringNew(bigbuf);
    sprintf(bigbuf,
      "            fmorphopgen_low_%d(datad, w, h, wpld, datat, wpls, index);",
      fileindex);
    str_low_dt = stringNew(bigbuf);
    sprintf(bigbuf,
      "            fmorphopgen_low_%d(datad, w, h, wpld, datas, wpls, index);",
      fileindex);
    str_low_ds = stringNew(bigbuf);
    sprintf(bigbuf,
     "            fmorphopgen_low_%d(datat, w, h, wpls, datas, wpls, index+1);",
      fileindex);
    str_low_tsp1 = stringNew(bigbuf);
    sprintf(bigbuf,
      "            fmorphopgen_low_%d(datat, w, h, wpls, datas, wpls, index);",
      fileindex);
    str_low_ts = stringNew(bigbuf);
    sprintf(bigbuf,
     "            fmorphopgen_low_%d(datad, w, h, wpld, datat, wpls, index+1);",
      fileindex);
    str_low_dtp1 = stringNew(bigbuf);

        /* Make the output sa */
    sa3 = sarrayCreate(0);

        /* Copyright notice and info header */
    sarrayParseRange(sa2, 0, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);

        /* Insert function names as documentation */
    sarrayAddString(sa3, str_doc1, L_INSERT);
    sarrayAddString(sa3, str_doc2, L_INSERT);

        /* Add '#include's */
    sarrayParseRange(sa2, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);

        /* Insert function prototypes */
    sarrayAddString(sa3, str_proto1, L_INSERT);
    sarrayAddString(sa3, str_proto2, L_INSERT);
    sarrayAddString(sa3, str_proto3, L_INSERT);

        /* Add static globals */
    sprintf(bigbuf, "\nstatic l_int32   NUM_SELS_GENERATED = %d;", nsels);
    sarrayAddString(sa3, bigbuf, L_COPY);
    sprintf(bigbuf, "static char  SEL_NAMES[][80] = {");
    sarrayAddString(sa3, bigbuf, L_COPY);
    for (i = 0; i < nsels - 1; i++) {
        sprintf(bigbuf, "                             \"%s\",",
                sarrayGetString(sa1, i, L_NOCOPY));
        sarrayAddString(sa3, bigbuf, L_COPY);
    }
    sprintf(bigbuf, "                             \"%s\"};",
            sarrayGetString(sa1, i, L_NOCOPY));
    sarrayAddString(sa3, bigbuf, L_COPY);

        /* Start pixMorphDwa_*() function description */
    sarrayParseRange(sa2, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);
    sarrayAddString(sa3, str_doc3, L_INSERT);
    sarrayParseRange(sa2, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);

        /* Finish pixMorphDwa_*() function definition */
    sarrayAddString(sa3, str_def1, L_INSERT);
    sarrayParseRange(sa2, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);
    sarrayAddString(sa3, str_proc1, L_INSERT);
    sarrayParseRange(sa2, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);
    sarrayAddString(sa3, str_dwa1, L_INSERT);
    sarrayParseRange(sa2, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);

        /* Start pixFMorphopGen_*() function description */
    sarrayAddString(sa3, str_doc4, L_INSERT);
    sarrayParseRange(sa2, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);

        /* Finish pixFMorphopGen_*() function definition */
    sarrayAddString(sa3, str_def2, L_INSERT);
    sarrayParseRange(sa2, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);
    sarrayAddString(sa3, str_proc2, L_INSERT);
    sarrayParseRange(sa2, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);
    sarrayAddString(sa3, str_low_dt, L_COPY);
    sarrayParseRange(sa2, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);
    sarrayAddString(sa3, str_low_ds, L_INSERT);
    sarrayParseRange(sa2, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);
    sarrayAddString(sa3, str_low_tsp1, L_INSERT);
    sarrayParseRange(sa2, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);
    sarrayAddString(sa3, str_low_dt, L_INSERT);
    sarrayParseRange(sa2, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);
    sarrayAddString(sa3, str_low_ts, L_INSERT);
    sarrayParseRange(sa2, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);
    sarrayAddString(sa3, str_low_dtp1, L_INSERT);
    sarrayParseRange(sa2, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);

        /* Output to file */
    filestr = sarrayToString(sa3, 1);
    nbytes = strlen(filestr);
    if (filename)
        snprintf(bigbuf, L_BUF_SIZE, "%s.%d.c", filename, fileindex);
    else
        sprintf(bigbuf, "%s.%d.c", OUTROOT, fileindex);
    l_binaryWrite(bigbuf, "w", filestr, nbytes);
    sarrayDestroy(&sa1);
    sarrayDestroy(&sa2);
    sarrayDestroy(&sa3);
    LEPT_FREE(filestr);
    return 0;
}


/*
 *  fmorphautogen2()
 *
 *      Input:  sela
 *              fileindex
 *              filename (<optional>; can be null)
 *      Return: 0 if OK; 1 on error
 *
 *  Notes:
 *      (1) This function uses morphtemplate2.txt to create a
 *          low-level file that contains the low-level functions for
 *          implementing dilation and erosion for every sel
 *          in the input sela.
 *      (2) The fileindex parameter is inserted into the output
 *          filename, as described below.
 *      (3) If filename == NULL, the output file is fmorphgenlow.[n].c,
 *          where [n] is equal to the 'fileindex' parameter.
 *      (4) If filename != NULL, the output file is [filename]low.[n].c.
 */
l_int32
fmorphautogen2(SELA        *sela,
               l_int32      fileindex,
               const char  *filename)
{
char    *filestr, *linestr, *fname;
char    *str_doc1, *str_doc2, *str_doc3, *str_doc4, *str_def1;
char     bigbuf[L_BUF_SIZE];
char     breakstring[] = "        break;";
char     staticstring[] = "static void";
l_int32  i, nsels, nbytes, actstart, end, newstart;
l_int32  argstart, argend, loopstart, loopend, finalstart, finalend;
size_t   size;
SARRAY  *sa1, *sa2, *sa3, *sa4, *sa5, *sa6;
SEL     *sel;

    if (!sela)
        return ERROR_INT("sela not defined", __func__, 1);
    if (fileindex < 0)
        fileindex = 0;
    if ((nsels = selaGetCount(sela)) == 0)
        return ERROR_INT("no sels in sela", __func__, 1);

        /* Make the array of textlines from morphtemplate2.txt */
    if ((filestr = (char *)l_binaryRead(TEMPLATE2, &size)) == NULL)
        return ERROR_INT("filestr not made", __func__, 1);
    sa1 = sarrayCreateLinesFromString(filestr, 1);
    LEPT_FREE(filestr);
    if (!sa1)
        return ERROR_INT("sa1 not made", __func__, 1);

        /* Make the array of static function names */
    if ((sa2 = sarrayCreate(2 * nsels)) == NULL) {
        sarrayDestroy(&sa1);
        return ERROR_INT("sa2 not made", __func__, 1);
    }
    for (i = 0; i < nsels; i++) {
        sprintf(bigbuf, "fdilate_%d_%d", fileindex, i);
        sarrayAddString(sa2, bigbuf, L_COPY);
        sprintf(bigbuf, "ferode_%d_%d", fileindex, i);
        sarrayAddString(sa2, bigbuf, L_COPY);
    }

        /* Make the static prototype strings */
    sa3 = sarrayCreate(2 * nsels);  /* should be ok */
    for (i = 0; i < 2 * nsels; i++) {
        fname = sarrayGetString(sa2, i, L_NOCOPY);
        sprintf(bigbuf, "static void  %s%s", fname, PROTOARGS);
        sarrayAddString(sa3, bigbuf, L_COPY);
    }

        /* Make strings containing function names */
    sprintf(bigbuf, " *             l_int32    fmorphopgen_low_%d()",
            fileindex);
    str_doc1 = stringNew(bigbuf);
    sprintf(bigbuf, " *             void       fdilate_%d_*()", fileindex);
    str_doc2 = stringNew(bigbuf);
    sprintf(bigbuf, " *             void       ferode_%d_*()", fileindex);
    str_doc3 = stringNew(bigbuf);

        /* Output to this sa */
    sa4 = sarrayCreate(0);

        /* Copyright notice and info header */
    sarrayParseRange(sa1, 0, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa4, sa1, actstart, end);

        /* Insert function names as documentation */
    sarrayAddString(sa4, str_doc1, L_INSERT);
    sarrayParseRange(sa1, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa4, sa1, actstart, end);
    sarrayAddString(sa4, str_doc2, L_INSERT);
    sarrayAddString(sa4, str_doc3, L_INSERT);
    sarrayParseRange(sa1, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa4, sa1, actstart, end);

        /* Insert static protos */
    for (i = 0; i < 2 * nsels; i++) {
        if ((linestr = sarrayGetString(sa3, i, L_COPY)) == NULL) {
            sarrayDestroy(&sa1);
            sarrayDestroy(&sa2);
            sarrayDestroy(&sa3);
            sarrayDestroy(&sa4);
            return ERROR_INT("linestr not retrieved", __func__, 1);
        }
        sarrayAddString(sa4, linestr, L_INSERT);
    }

        /* More strings with function names */
    sprintf(bigbuf, " *  fmorphopgen_low_%d()", fileindex);
    str_doc4 = stringNew(bigbuf);
    sprintf(bigbuf, "fmorphopgen_low_%d(l_uint32  *datad,", fileindex);
    str_def1 = stringNew(bigbuf);

        /* Insert function header */
    sarrayParseRange(sa1, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa4, sa1, actstart, end);
    sarrayAddString(sa4, str_doc4, L_INSERT);
    sarrayParseRange(sa1, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa4, sa1, actstart, end);
    sarrayAddString(sa4, str_def1, L_INSERT);
    sarrayParseRange(sa1, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa4, sa1, actstart, end);

        /* Generate and insert the dispatcher code */
    for (i = 0; i < 2 * nsels; i++) {
        sprintf(bigbuf, "    case %d:", i);
        sarrayAddString(sa4, bigbuf, L_COPY);
        sprintf(bigbuf, "        %s(datad, w, h, wpld, datas, wpls);",
               sarrayGetString(sa2, i, L_NOCOPY));
        sarrayAddString(sa4, bigbuf, L_COPY);
        sarrayAddString(sa4, breakstring, L_COPY);
    }

        /* Finish the dispatcher and introduce the low-level code */
    sarrayParseRange(sa1, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa4, sa1, actstart, end);

        /* Get the range for the args common to all functions */
    sarrayParseRange(sa1, newstart, &argstart, &argend, &newstart, "--", 0);

        /* Get the range for the loop code common to all functions */
    sarrayParseRange(sa1, newstart, &loopstart, &loopend, &newstart, "--", 0);

        /* Get the range for the ending code common to all functions */
    sarrayParseRange(sa1, newstart, &finalstart, &finalend, &newstart, "--", 0);

        /* Do all the static functions */
    for (i = 0; i < 2 * nsels; i++) {
            /* Generate the function header and add the common args */
        sarrayAddString(sa4, staticstring, L_COPY);
        fname = sarrayGetString(sa2, i, L_NOCOPY);
        sprintf(bigbuf, "%s(l_uint32  *datad,", fname);
        sarrayAddString(sa4, bigbuf, L_COPY);
        sarrayAppendRange(sa4, sa1, argstart, argend);

            /* Declare and define wplsN args, as necessary */
        if ((sel = selaGetSel(sela, i/2)) == NULL) {
            sarrayDestroy(&sa1);
            sarrayDestroy(&sa2);
            sarrayDestroy(&sa3);
            sarrayDestroy(&sa4);
            return ERROR_INT("sel not returned", __func__, 1);
        }
        sa5 = sarrayMakeWplsCode(sel);
        sarrayJoin(sa4, sa5);
        sarrayDestroy(&sa5);

            /* Add the function loop code */
        sarrayAppendRange(sa4, sa1, loopstart, loopend);

            /* Insert barrel-op code for *dptr */
        sa6 = sarrayMakeInnerLoopDWACode(sel, i);
        sarrayJoin(sa4, sa6);
        sarrayDestroy(&sa6);

            /* Finish the function code */
        sarrayAppendRange(sa4, sa1, finalstart, finalend);
    }

        /* Output to file */
    filestr = sarrayToString(sa4, 1);
    nbytes = strlen(filestr);
    if (filename)
        snprintf(bigbuf, L_BUF_SIZE, "%slow.%d.c", filename, fileindex);
    else
        sprintf(bigbuf, "%slow.%d.c", OUTROOT, fileindex);
    l_binaryWrite(bigbuf, "w", filestr, nbytes);
    sarrayDestroy(&sa1);
    sarrayDestroy(&sa2);
    sarrayDestroy(&sa3);
    sarrayDestroy(&sa4);
    LEPT_FREE(filestr);
    return 0;
}


/*--------------------------------------------------------------------------*
 *                            Helper code for sel                           *
 *--------------------------------------------------------------------------*/
/*!
 * \brief   sarrayMakeWplsCode()
 */
static SARRAY *
sarrayMakeWplsCode(SEL  *sel)
{
char     emptystring[] = "";
l_int32  i, j, ymax, dely, allvshifts;
l_int32  vshift[32];
SARRAY  *sa;

    if (!sel)
        return (SARRAY *)ERROR_PTR("sel not defined", __func__, NULL);

    for (i = 0; i < 32; i++)
        vshift[i] = 0;
    ymax = 0;
    for (i = 0; i < sel->sy; i++) {
        for (j = 0; j < sel->sx; j++) {
            if (sel->data[i][j] == 1) {
                dely = L_ABS(i - sel->cy);
                if (dely < 32)
                    vshift[dely] = 1;
                ymax = L_MAX(ymax, dely);
            }
        }
    }
    if (ymax > 31) {
        L_WARNING("ymax > 31; truncating to 31\n", __func__);
        ymax = 31;
    }

        /* Test if this is a vertical brick */
    allvshifts = TRUE;
    for (i = 0; i < ymax; i++) {
        if (vshift[i] == 0) {
            allvshifts = FALSE;
            break;
        }
    }

    sa = sarrayCreate(0);

        /* Add declarations */
    if (allvshifts == TRUE) {   /* packs them as well as possible */
        if (ymax > 4)
            sarrayAddString(sa, wpldecls[2], L_COPY);
        if (ymax > 8)
            sarrayAddString(sa, wpldecls[6], L_COPY);
        if (ymax > 12)
            sarrayAddString(sa, wpldecls[10], L_COPY);
        if (ymax > 16)
            sarrayAddString(sa, wpldecls[14], L_COPY);
        if (ymax > 20)
            sarrayAddString(sa, wpldecls[18], L_COPY);
        if (ymax > 24)
            sarrayAddString(sa, wpldecls[22], L_COPY);
        if (ymax > 28)
            sarrayAddString(sa, wpldecls[26], L_COPY);
        if (ymax > 1)
            sarrayAddString(sa, wpldecls[ymax - 2], L_COPY);
    } else {  /* puts them one/line */
        for (i = 2; i <= ymax; i++) {
            if (vshift[i])
                sarrayAddString(sa, wplgendecls[i - 2], L_COPY);
        }
    }

    sarrayAddString(sa, emptystring, L_COPY);

        /* Add definitions */
    for (i = 2; i <= ymax; i++) {
        if (vshift[i])
            sarrayAddString(sa, wpldefs[i - 2], L_COPY);
    }

    return sa;
}


/*!
 * \brief   sarrayMakeInnerLoopDWACode()
 */
static SARRAY *
sarrayMakeInnerLoopDWACode(SEL     *sel,
                           l_int32  index)
{
char    *tstr, *string;
char     logicalor[] = "|";
char     logicaland[] = "&";
char     bigbuf[L_BUF_SIZE];
l_int32  i, j, optype, count, nfound, delx, dely;
SARRAY  *sa;

    if (!sel)
        return (SARRAY *)ERROR_PTR("sel not defined", __func__, NULL);

    if (index % 2 == 0) {
        optype = L_MORPH_DILATE;
        tstr = logicalor;
    } else {
        optype = L_MORPH_ERODE;
        tstr = logicaland;
    }

    count = 0;
    for (i = 0; i < sel->sy; i++) {
        for (j = 0; j < sel->sx; j++) {
            if (sel->data[i][j] == 1)
                count++;
        }
    }

    sa = sarrayCreate(0);
    if (count == 0) {
        L_WARNING("no hits in Sel %d\n", __func__, index);
        return sa;  /* no code inside! */
    }

    nfound = 0;
    for (i = 0; i < sel->sy; i++) {
        for (j = 0; j < sel->sx; j++) {
            if (sel->data[i][j] == 1) {
                nfound++;
                if (optype == L_MORPH_DILATE) {
                    dely = sel->cy - i;
                    delx = sel->cx - j;
                } else {  /* optype == L_MORPH_ERODE */
                    dely = i - sel->cy;
                    delx = j - sel->cx;
                }
                if ((string = makeBarrelshiftString(delx, dely)) == NULL) {
                    L_WARNING("barrel shift string not made\n", __func__);
                    continue;
                }
                if (count == 1)  /* just one item */
                    sprintf(bigbuf, "            *dptr = %s;", string);
                else if (nfound == 1)
                    sprintf(bigbuf, "            *dptr = %s %s", string, tstr);
                else if (nfound < count)
                    sprintf(bigbuf, "                    %s %s", string, tstr);
                else  /* nfound == count */
                    sprintf(bigbuf, "                    %s;", string);
                sarrayAddString(sa, bigbuf, L_COPY);
                LEPT_FREE(string);
            }
        }
    }

    return sa;
}


/*!
 * \brief   makeBarrelshiftString()
 */
static char *
makeBarrelshiftString(l_int32  delx,    /* j - cx */
                      l_int32  dely)    /* i - cy */
{
l_int32  absx, absy;
char     bigbuf[L_BUF_SIZE];

    if (delx < -31 || delx > 31)
        return (char *)ERROR_PTR("delx out of bounds", __func__, NULL);
    if (dely < -31 || dely > 31)
        return (char *)ERROR_PTR("dely out of bounds", __func__, NULL);
    absx = L_ABS(delx);
    absy = L_ABS(dely);

    if ((delx == 0) && (dely == 0))
        sprintf(bigbuf, "(*sptr)");
    else if ((delx == 0) && (dely < 0))
        sprintf(bigbuf, "(*(sptr %s))", wplstrm[absy - 1]);
    else if ((delx == 0) && (dely > 0))
        sprintf(bigbuf, "(*(sptr %s))", wplstrp[absy - 1]);
    else if ((delx < 0) && (dely == 0))
        sprintf(bigbuf, "((*(sptr) >> %d) | (*(sptr - 1) << %d))",
              absx, 32 - absx);
    else if ((delx > 0) && (dely == 0))
        sprintf(bigbuf, "((*(sptr) << %d) | (*(sptr + 1) >> %d))",
              absx, 32 - absx);
    else if ((delx < 0) && (dely < 0))
        sprintf(bigbuf, "((*(sptr %s) >> %d) | (*(sptr %s - 1) << %d))",
              wplstrm[absy - 1], absx, wplstrm[absy - 1], 32 - absx);
    else if ((delx > 0) && (dely < 0))
        sprintf(bigbuf, "((*(sptr %s) << %d) | (*(sptr %s + 1) >> %d))",
              wplstrm[absy - 1], absx, wplstrm[absy - 1], 32 - absx);
    else if ((delx < 0) && (dely > 0))
        sprintf(bigbuf, "((*(sptr %s) >> %d) | (*(sptr %s - 1) << %d))",
              wplstrp[absy - 1], absx, wplstrp[absy - 1], 32 - absx);
    else  /*  ((delx > 0) && (dely > 0))  */
        sprintf(bigbuf, "((*(sptr %s) << %d) | (*(sptr %s + 1) >> %d))",
              wplstrp[absy - 1], absx, wplstrp[absy - 1], 32 - absx);

    return stringNew(bigbuf);
}
