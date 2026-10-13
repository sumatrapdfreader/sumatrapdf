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
 * \file fhmtauto.c
 * <pre>
 *
 *    Main function calls:
 *       l_int32             fhmtautogen()
 *       l_int32             fhmtautogen1()
 *       l_int32             fhmtautogen2()
 *
 *    Static helpers:
 *       static SARRAY      *sarrayMakeWplsCode()
 *       static SARRAY      *sarrayMakeInnerLoopDWACode()
 *       static char        *makeBarrelshiftString()
 *
 *    This automatically generates dwa code for the hit-miss transform.
 *    Here's a road map for how it all works.
 *
 *    (1) You generate an array (a SELA) of hit-miss transform SELs.
 *        This can be done in several ways, including
 *           (a) calling the function selaAddHitMiss() for
 *               pre-compiled SELs
 *           (b) generating the SELA in code in line
 *           (c) reading in a SELA from file, using selaRead()
 *               or various other formats.
 *
 *    (2) You call fhmtautogen1() and fhmtautogen2() on this SELA.
 *        This uses the text files hmttemplate1.txt and
 *        hmttemplate2.txt for building up the source code.  See the file
 *        prog/fhmtautogen.c for an example of how this is done.
 *        The output is written to files named fhmtgen.*.c
 *        and fhmtgenlow.*.c, where "*" is an integer that you
 *        input to this function.  That integer labels both
 *        the output files, as well as all the functions that
 *        are generated.  That way, using different integers,
 *        you can invoke fhmtautogen() any number of times
 *        to get functions that all have different names so that
 *        they can be linked into one program.
 *
 *    (3) You copy the generated source code back to your src
 *        directory for compilation.  Put their names in the
 *        Makefile, regnerate the prototypes, and recompile
 *        the libraries.  Look at the Makefile to see how I've
 *        included fhmtgen.1.c and fhmtgenlow.1.c.  These files
 *        provide the single high-level interface for the hmt, and
 *        the lower-level functions to do the actual work.
 *
 *    (4) In an application, you now use this interface.  Again
 *        for the example files generated, using integer "1":
 *
 *           PIX   *pixHMTDwa_1(PIX *pixd, PIX *pixs, const char *selname);
 *
 *        where the selname is one of the set that were defined
 *        as the name field of sels.  This set is listed at the
 *        beginning of the file fhmtgen.1.c.
 *
 *        N.B. Although pixFHMTGen_1() is global, you must NOT
 *        use it, because it assumes that 32 or 64 border pixels
 *        have been added to each side, and it will crash without those
 *        added pixels.
 *
 *        As an example, see the file prog/fhmtauto_reg.c, which
 *        verifies the correctness of the implementation by
 *        comparing the dwa result with that of full-image
 *        rasterops.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"

#define   OUTROOT         "fhmtgen"
#define   TEMPLATE1       "hmttemplate1.txt"
#define   TEMPLATE2       "hmttemplate2.txt"

#define   PROTOARGS   "(l_uint32 *, l_int32, l_int32, l_int32, l_uint32 *, l_int32);"

#define L_BUF_SIZE 512

static char * makeBarrelshiftString(l_int32 delx, l_int32 dely, l_int32 type);
static SARRAY * sarrayMakeInnerLoopDWACode(SEL *sel, l_int32 nhits, l_int32 nmisses);
static SARRAY * sarrayMakeWplsCode(SEL *sel);

static char wpldecls[][60] = {
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

static char wpldefs[][24] = {
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
 * \brief   fhmtautogen()
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
 *      (2) See fhmtautogen1() and fhmtautogen2() for details.
 * </pre>
 */
l_ok
fhmtautogen(SELA        *sela,
            l_int32      fileindex,
            const char  *filename)
{
l_int32  ret1, ret2;

    if (!sela)
        return ERROR_INT("sela not defined", __func__, 1);
    ret1 = fhmtautogen1(sela, fileindex, filename);
    ret2 = fhmtautogen2(sela, fileindex, filename);
    if (ret1 || ret2)
        return ERROR_INT("code generation problem", __func__, 1);
    return 0;
}


/*!
 * \brief   fhmtautogen1()
 *
 * \param[in]    sela array
 * \param[in]    fileindex
 * \param[in]    filename [optional]; can be null
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This function uses hmttemplate1.txt to create a
 *          top-level file that contains two functions that carry
 *          out the hit-miss transform for any of the sels in
 *          the input sela.
 *      (2) The fileindex parameter is inserted into the output
 *          filename, as described below.
 *      (3) If filename == NULL, the output file is fhmtgen.[n].c,
 *          where [n] is equal to the 'fileindex' parameter.
 *      (4) If filename != NULL, the output file is [filename].[n].c.
 *      (5) Each sel must have at least one hit.  A sel with only misses
 *          generates code that will abort the operation if it is called.
 * </pre>
 */
l_ok
fhmtautogen1(SELA        *sela,
             l_int32      fileindex,
             const char  *filename)
{
char    *filestr;
char    *str_proto1, *str_proto2, *str_proto3;
char    *str_doc1, *str_doc2, *str_doc3, *str_doc4;
char    *str_def1, *str_def2, *str_proc1, *str_proc2;
char    *str_dwa1, *str_low_dt, *str_low_ds;
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

        /* Make array of textlines from from hmttemplate1.txt */
    if ((filestr = (char *)l_binaryRead(TEMPLATE1, &size)) == NULL)
        return ERROR_INT("filestr not made", __func__, 1);
    sa2 = sarrayCreateLinesFromString(filestr, 1);
    LEPT_FREE(filestr);
    if (!sa2)
        return ERROR_INT("sa2 not made", __func__, 1);

        /* Make array of sel names */
    sa1 = selaGetSelnames(sela);

        /* Make strings containing function call names */
    sprintf(bigbuf, "PIX *pixHMTDwa_%d(PIX *pixd, PIX *pixs, "
                    "const char *selname);", fileindex);
    str_proto1 = stringNew(bigbuf);
    sprintf(bigbuf, "PIX *pixFHMTGen_%d(PIX *pixd, PIX *pixs, "
                    "const char *selname);", fileindex);
    str_proto2 = stringNew(bigbuf);
    sprintf(bigbuf, "l_int32 fhmtgen_low_%d(l_uint32 *datad, l_int32 w,\n"
            "                      l_int32 h, l_int32 wpld,\n"
            "                      l_uint32 *datas, l_int32 wpls,\n"
            "                      l_int32 index);", fileindex);
    str_proto3 = stringNew(bigbuf);
    sprintf(bigbuf, " *             PIX     *pixHMTDwa_%d()", fileindex);
    str_doc1 = stringNew(bigbuf);
    sprintf(bigbuf, " *             PIX     *pixFHMTGen_%d()", fileindex);
    str_doc2 = stringNew(bigbuf);
    sprintf(bigbuf, " * \\brief  pixHMTDwa_%d()", fileindex);
    str_doc3 = stringNew(bigbuf);
    sprintf(bigbuf, " * \\brief  pixFHMTGen_%d()", fileindex);
    str_doc4 = stringNew(bigbuf);
    sprintf(bigbuf, "pixHMTDwa_%d(PIX         *pixd,", fileindex);
    str_def1 = stringNew(bigbuf);
    sprintf(bigbuf, "pixFHMTGen_%d(PIX         *pixd,", fileindex);
    str_def2 = stringNew(bigbuf);
    sprintf(bigbuf, "    PROCNAME(\"pixHMTDwa_%d\");", fileindex);
    str_proc1 = stringNew(bigbuf);
    sprintf(bigbuf, "    PROCNAME(\"pixFHMTGen_%d\");", fileindex);
    str_proc2 = stringNew(bigbuf);
    sprintf(bigbuf, "    pixt2 = pixFHMTGen_%d(NULL, pixt1, selname);",
            fileindex);
    str_dwa1 = stringNew(bigbuf);
    sprintf(bigbuf,
            "        fhmtgen_low_%d(datad, w, h, wpld, datat, wpls, index);",
            fileindex);
    str_low_dt = stringNew(bigbuf);
    sprintf(bigbuf,
            "        fhmtgen_low_%d(datad, w, h, wpld, datas, wpls, index);",
            fileindex);
    str_low_ds = stringNew(bigbuf);

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

        /* Start pixHMTDwa_*() function description */
    sarrayParseRange(sa2, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);
    sarrayAddString(sa3, str_doc3, L_INSERT);
    sarrayParseRange(sa2, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);

        /* Finish pixHMTDwa_*() function definition */
    sarrayAddString(sa3, str_def1, L_INSERT);
    sarrayParseRange(sa2, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);
    sarrayAddString(sa3, str_proc1, L_INSERT);
    sarrayParseRange(sa2, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);
    sarrayAddString(sa3, str_dwa1, L_INSERT);
    sarrayParseRange(sa2, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);

        /* Start pixFHMTGen_*() function description */
    sarrayAddString(sa3, str_doc4, L_INSERT);
    sarrayParseRange(sa2, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);

        /* Finish pixFHMTGen_*() function description */
    sarrayAddString(sa3, str_def2, L_INSERT);
    sarrayParseRange(sa2, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);
    sarrayAddString(sa3, str_proc2, L_INSERT);
    sarrayParseRange(sa2, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);
    sarrayAddString(sa3, str_low_dt, L_INSERT);
    sarrayParseRange(sa2, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);
    sarrayAddString(sa3, str_low_ds, L_INSERT);
    sarrayParseRange(sa2, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);

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


/*!
 * \brief   fhmtautogen2()
 *
 * \param[in]    sela array
 * \param[in]    fileindex
 * \param[in]    filename [optional]; can be null
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This function uses hmttemplate2.txt to create a
 *          low-level file that contains the low-level functions for
 *          implementing the hit-miss transform for every sel
 *          in the input sela.
 *      (2) The fileindex parameter is inserted into the output
 *          filename, as described below.
 *      (3) If filename == NULL, the output file is fhmtgenlow.[n].c,
 *          where [n] is equal to the %fileindex parameter.
 *      (4) If filename != NULL, the output file is [filename]low.[n].c.
 * </pre>
 */
l_ok
fhmtautogen2(SELA        *sela,
             l_int32      fileindex,
             const char  *filename)
{
char    *filestr, *fname, *linestr;
char    *str_doc1, *str_doc2, *str_doc3, *str_def1;
char     bigbuf[L_BUF_SIZE];
char     breakstring[] = "        break;";
char     staticstring[] = "static void";
l_int32  i, k, l, nsels, nbytes, nhits, nmisses;
l_int32  actstart, end, newstart;
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

        /* Make the array of textlines from hmttemplate2.txt */
    if ((filestr = (char *)l_binaryRead(TEMPLATE2, &size)) == NULL)
        return ERROR_INT("filestr not made", __func__, 1);
    sa1 = sarrayCreateLinesFromString(filestr, 1);
    LEPT_FREE(filestr);
    if (!sa1)
        return ERROR_INT("sa1 not made", __func__, 1);

        /* Make the array of static function names */
    if ((sa2 = sarrayCreate(nsels)) == NULL) {
        sarrayDestroy(&sa1);
        return ERROR_INT("sa2 not made", __func__, 1);
    }
    for (i = 0; i < nsels; i++) {
        sprintf(bigbuf, "fhmt_%d_%d", fileindex, i);
        sarrayAddString(sa2, bigbuf, L_COPY);
    }

        /* Make the static prototype strings */
    sa3 = sarrayCreate(2 * nsels);  /* should be ok */
    for (i = 0; i < nsels; i++) {
        fname = sarrayGetString(sa2, i, L_NOCOPY);
        sprintf(bigbuf, "static void  %s%s", fname, PROTOARGS);
        sarrayAddString(sa3, bigbuf, L_COPY);
    }

        /* Make strings containing function names */
    sprintf(bigbuf, " *             l_int32    fhmtgen_low_%d()",
            fileindex);
    str_doc1 = stringNew(bigbuf);
    sprintf(bigbuf, " *             void       fhmt_%d_*()", fileindex);
    str_doc2 = stringNew(bigbuf);

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
    sarrayParseRange(sa1, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa4, sa1, actstart, end);

        /* Insert static protos */
    for (i = 0; i < nsels; i++) {
        if ((linestr = sarrayGetString(sa3, i, L_COPY)) == NULL) {
            sarrayDestroy(&sa1);
            sarrayDestroy(&sa2);
            sarrayDestroy(&sa3);
            sarrayDestroy(&sa4);
            return ERROR_INT("linestr not retrieved", __func__, 1);
        }
        sarrayAddString(sa4, linestr, L_INSERT);
    }

        /* Make more strings containing function names */
    sprintf(bigbuf, " *  fhmtgen_low_%d()", fileindex);
    str_doc3 = stringNew(bigbuf);
    sprintf(bigbuf, "fhmtgen_low_%d(l_uint32  *datad,", fileindex);
    str_def1 = stringNew(bigbuf);

        /* Insert function header */
    sarrayParseRange(sa1, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa4, sa1, actstart, end);
    sarrayAddString(sa4, str_doc3, L_INSERT);
    sarrayParseRange(sa1, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa4, sa1, actstart, end);
    sarrayAddString(sa4, str_def1, L_INSERT);
    sarrayParseRange(sa1, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa4, sa1, actstart, end);

        /* Generate and insert the dispatcher code */
    for (i = 0; i < nsels; i++) {
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
    for (i = 0; i < nsels; i++) {
            /* Generate the function header and add the common args */
        sarrayAddString(sa4, staticstring, L_COPY);
        fname = sarrayGetString(sa2, i, L_NOCOPY);
        sprintf(bigbuf, "%s(l_uint32  *datad,", fname);
        sarrayAddString(sa4, bigbuf, L_COPY);
        sarrayAppendRange(sa4, sa1, argstart, argend);

            /* Declare and define wplsN args, as necessary */
        if ((sel = selaGetSel(sela, i)) == NULL) {
            sarrayDestroy(&sa1);
            sarrayDestroy(&sa2);
            sarrayDestroy(&sa3);
            sarrayDestroy(&sa4);
            return ERROR_INT("sel not returned", __func__, 1);
        }
        sa5 = sarrayMakeWplsCode(sel);
        sarrayJoin(sa4, sa5);
        sarrayDestroy(&sa5);

            /* Make sure sel has at least one hit */
        nhits = 0;
        nmisses = 0;
        for (k = 0; k < sel->sy; k++) {
            for (l = 0; l < sel->sx; l++) {
                if (sel->data[k][l] == 1)
                    nhits++;
                else if (sel->data[k][l] == 2)
                    nmisses++;
            }
        }
        if (nhits == 0) {
            linestr = stringNew("    "
                "lept_stderr(\"Error in HMT: no hits in sel!\\n\");\n}\n\n");
            sarrayAddString(sa4, linestr, L_INSERT);
            continue;
        }

            /* Add the function loop code */
        sarrayAppendRange(sa4, sa1, loopstart, loopend);

            /* Insert barrel-op code for *dptr */
        if ((sa6 = sarrayMakeInnerLoopDWACode(sel, nhits, nmisses)) == NULL) {
            sarrayDestroy(&sa1);
            sarrayDestroy(&sa2);
            sarrayDestroy(&sa3);
            sarrayDestroy(&sa4);
            return ERROR_INT("sa6 not made", __func__, 1);
        }
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
l_int32  i, j, ymax, dely;
SARRAY  *sa;

    if (!sel)
        return (SARRAY *)ERROR_PTR("sel not defined", __func__, NULL);

    ymax = 0;
    for (i = 0; i < sel->sy; i++) {
        for (j = 0; j < sel->sx; j++) {
            if (sel->data[i][j] == 1 || sel->data[i][j] == 2) {
                dely = L_ABS(i - sel->cy);
                ymax = L_MAX(ymax, dely);
            }
        }
    }
    if (ymax > 31) {
        L_WARNING("ymax > 31; truncating to 31\n", __func__);
        ymax = 31;
    }

    sa = sarrayCreate(0);

        /* Declarations */
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

    sarrayAddString(sa, emptystring, L_COPY);

        /* Definitions */
    for (i = 2; i <= ymax; i++)
        sarrayAddString(sa, wpldefs[i - 2], L_COPY);

    return sa;
}


/*!
 * \brief   sarrayMakeInnerLoopDWACode()
 */
static SARRAY *
sarrayMakeInnerLoopDWACode(SEL     *sel,
                           l_int32  nhits,
                           l_int32  nmisses)
{
char    *string;
char     land[] = "&";
char     bigbuf[L_BUF_SIZE];
l_int32  i, j, ntot, nfound, type, delx, dely;
SARRAY  *sa;

    if (!sel)
        return (SARRAY *)ERROR_PTR("sel not defined", __func__, NULL);

    sa = sarrayCreate(0);
    ntot = nhits + nmisses;
    nfound = 0;
    for (i = 0; i < sel->sy; i++) {
        for (j = 0; j < sel->sx; j++) {
            type = sel->data[i][j];
            if (type == SEL_HIT || type == SEL_MISS) {
                nfound++;
                dely = i - sel->cy;
                delx = j - sel->cx;
                if ((string = makeBarrelshiftString(delx, dely, type))
                        == NULL) {
                    L_WARNING("barrel shift string not made\n", __func__);
                    continue;
                }
                if (ntot == 1)  /* just one item */
                    sprintf(bigbuf, "            *dptr = %s;", string);
                else if (nfound == 1)
                    sprintf(bigbuf, "            *dptr = %s %s", string, land);
                else if (nfound < ntot)
                    sprintf(bigbuf, "                    %s %s", string, land);
                else  /* nfound == ntot */
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
                      l_int32  dely,    /* i - cy */
                      l_int32  type)    /* SEL_HIT or SEL_MISS */
{
l_int32  absx, absy;
char     bigbuf[L_BUF_SIZE];

    if (delx < -31 || delx > 31)
        return (char *)ERROR_PTR("delx out of bounds", __func__, NULL);
    if (dely < -31 || dely > 31)
        return (char *)ERROR_PTR("dely out of bounds", __func__, NULL);
    absx = L_ABS(delx);
    absy = L_ABS(dely);

    if (type == SEL_HIT) {
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
    } else {  /* type == SEL_MISS */
        if ((delx == 0) && (dely == 0))
            sprintf(bigbuf, "(~*sptr)");
        else if ((delx == 0) && (dely < 0))
            sprintf(bigbuf, "(~*(sptr %s))", wplstrm[absy - 1]);
        else if ((delx == 0) && (dely > 0))
            sprintf(bigbuf, "(~*(sptr %s))", wplstrp[absy - 1]);
        else if ((delx < 0) && (dely == 0))
            sprintf(bigbuf, "((~*(sptr) >> %d) | (~*(sptr - 1) << %d))",
                  absx, 32 - absx);
        else if ((delx > 0) && (dely == 0))
            sprintf(bigbuf, "((~*(sptr) << %d) | (~*(sptr + 1) >> %d))",
                  absx, 32 - absx);
        else if ((delx < 0) && (dely < 0))
            sprintf(bigbuf, "((~*(sptr %s) >> %d) | (~*(sptr %s - 1) << %d))",
                  wplstrm[absy - 1], absx, wplstrm[absy - 1], 32 - absx);
        else if ((delx > 0) && (dely < 0))
            sprintf(bigbuf, "((~*(sptr %s) << %d) | (~*(sptr %s + 1) >> %d))",
                  wplstrm[absy - 1], absx, wplstrm[absy - 1], 32 - absx);
        else if ((delx < 0) && (dely > 0))
            sprintf(bigbuf, "((~*(sptr %s) >> %d) | (~*(sptr %s - 1) << %d))",
                  wplstrp[absy - 1], absx, wplstrp[absy - 1], 32 - absx);
        else  /*  ((delx > 0) && (dely > 0))  */
            sprintf(bigbuf, "((~*(sptr %s) << %d) | (~*(sptr %s + 1) >> %d))",
                  wplstrp[absy - 1], absx, wplstrp[absy - 1], 32 - absx);
    }

    return stringNew(bigbuf);
}
