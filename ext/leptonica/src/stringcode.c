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
 * \file stringcode.c
 * <pre>
 *
 *   Generation of code for storing and extracting serializable
 *   leptonica objects (such as pixa, recog, ...).
 *
 *   The input is a set of files with serialized data.
 *   The output is two files, that must be compiled and linked:
 *     ~ autogen.*.c: code for base64 unencoding the strings and
 *                    deserializing the result.
 *     ~ autogen.*.h: function prototypes and base64 encoded strings
 *                    of the input data
 *
 *   This should work for any data structures in leptonica that have
 *   *Write() and *Read() serialization functions.  An array of 20
 *   of these, including the Pix, is given below.  (The Pix is a special
 *   case, because it is serialized by standardized compression
 *   techniques, instead of a file format determined by leptonica.)
 *
 *   Each time the generator function is invoked, three sets of strings are
 *   produced, which are written into their respective string arrays:
 *     ~ string of serialized, gzipped and base 64 encoded data
 *     ~ case string for base64 decoding, gunzipping and deserialization,
 *       to return the data struct in memory
 *     ~ description string for selecting which struct to return
 *   To create the two output files, a finalize function is invoked.
 *
 *   There are two ways to do this, both shown in prog/autogentest1.c.
 *     ~ Explicitly call strcodeGenerate() for each file with the
 *       serialized data structure, followed by strcodeFinalize().
 *     ~ Put the filenames of the serialized data structures in a file,
 *       and call strcodeCreateFromFile().
 *
 *   The generated code in autogen.X.c and autogen.X.h (where X is an
 *   integer supplied to strcodeCreate()) is then compiled, and the
 *   original data can be regenerated using the function l_autodecode_X().
 *   A test example is found in the two prog files:
 *       prog/autogentest1.c  -- generates autogen.137.c, autogen.137.h
 *       prog/autogentest2.c  -- uses autogen.137.c, autogen.137.h
 *   In general, the generator (e.g., autogentest1) would be compiled and
 *   run before compiling and running the application (e.g., autogentest2).
 *
 *       L_STRCODE       *strcodeCreate()
 *       static void      strcodeDestroy()    (called as part of finalize)
 *       void             strcodeCreateFromFile()
 *       l_int32          strcodeGenerate()
 *       l_int32          strcodeFinalize()
 *       l_int32          l_getStructStrFromFile()   (useful externally)
 *
 *   Static helpers
 *       static l_int32   l_getIndexFromType()
 *       static l_int32   l_getIndexFromStructname()
 *       static l_int32   l_getIndexFromFile()
 *       static char     *l_genDataString()
 *       static char     *l_genCaseString()
 *       static char     *l_genDescrString()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"
#include "stringcode.h"

#define TEMPLATE1  "stringtemplate1.txt"  /* for assembling autogen.*.c */
#define TEMPLATE2  "stringtemplate2.txt"  /* for assembling autogen.*.h */

    /*! Associations between names and functions */
struct L_GenAssoc
{
    l_int32  index;
    char     type[16];        /* e.g., "PIXA" */
    char     structname[16];  /* e.g., "Pixa" */
    char     reader[16];      /* e.g., "pixaRead" */
    char     memreader[20];   /* e.g., "pixaReadMem" */
};

    /*! Number of serializable data types */
static const l_int32  l_ntypes = 19;
    /*! Serializable data types */
static const struct L_GenAssoc l_assoc[] = {
    {0,  "INVALID",     "invalid",     "invalid",        "invalid"         },
    {1,  "BOXA",        "Boxa",        "boxaRead",       "boxaReadMem"     },
    {2,  "BOXAA",       "Boxaa",       "boxaaRead",      "boxaaReadMem"    },
    {3,  "L_DEWARP",    "Dewarp",      "dewarpRead",     "dewarpReadMem"   },
    {4,  "L_DEWARPA",   "Dewarpa",     "dewarpaRead",    "dewarpaReadMem"  },
    {5,  "L_DNA",       "L_Dna",       "l_dnaRead",      "l_dnaReadMem"    },
    {6,  "L_DNAA",      "L_Dnaa",      "l_dnaaRead",     "l_dnaaReadMem"   },
    {7,  "DPIX",        "DPix",        "dpixRead",       "dpixReadMem"     },
    {8,  "FPIX",        "FPix",        "fpixRead",       "fpixReadMem"     },
    {9,  "NUMA",        "Numa",        "numaRead",       "numaReadMem"     },
    {10, "NUMAA",       "Numaa",       "numaaRead",      "numaaReadMem"    },
    {11, "PIX",         "Pix",         "pixRead",        "pixReadMem"      },
    {12, "PIXA",        "Pixa",        "pixaRead",       "pixaReadMem"     },
    {13, "PIXAA",       "Pixaa",       "pixaaRead",      "pixaaReadMem"    },
    {14, "PIXACOMP",    "Pixacomp",    "pixacompRead",   "pixacompReadMem" },
    {15, "PIXCMAP",     "Pixcmap",     "pixcmapRead",    "pixcmapReadMem"  },
    {16, "PTA",         "Pta",         "ptaRead",        "ptaReadMem"      },
    {17, "PTAA",        "Ptaa",        "ptaaRead",       "ptaaReadMem"     },
    {18, "RECOG",       "Recog",       "recogRead",      "recogReadMem"    },
    {19, "SARRAY",      "Sarray",      "sarrayRead",     "sarrayReadMem"   }
};

static l_int32 l_getIndexFromType(const char *type, l_int32 *pindex);
static l_int32 l_getIndexFromStructname(const char *sn, l_int32 *pindex);
static l_int32 l_getIndexFromFile(const char *file, l_int32 *pindex);
static char *l_genDataString(const char *filein, l_int32 ifunc);
static char *l_genCaseString(l_int32 ifunc, l_int32 itype);
static char *l_genDescrString(const char *filein, l_int32 ifunc, l_int32 itype);

/*---------------------------------------------------------------------*/
/*                         Stringcode functions                        */
/*---------------------------------------------------------------------*/
/*!
 * \brief   strcodeCreate()
 *
 * \param[in]    fileno    integer that labels the two output files
 * \return  initialized L_StrCode, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This struct exists to build two files containing code for
 *          any number of data objects.  The two files are named
 *             autogen.[fileno].c
 *             autogen.[fileno].h
 * </pre>
 */
L_STRCODE *
strcodeCreate(l_int32  fileno)
{
L_STRCODE  *strcode;

    lept_mkdir("lept/auto");

    if ((strcode = (L_STRCODE *)LEPT_CALLOC(1, sizeof(L_STRCODE))) == NULL)
        return (L_STRCODE *)ERROR_PTR("strcode not made", __func__, NULL);

    strcode->fileno = fileno;
    strcode->function = sarrayCreate(0);
    strcode->data = sarrayCreate(0);
    strcode->descr = sarrayCreate(0);
    return strcode;
}


/*!
 * \brief   strcodeDestroy()
 *
 * \param[out]  pstrcode    will be set to null after destroying the sarrays
 * \return  void
 */
static void
strcodeDestroy(L_STRCODE  **pstrcode)
{
L_STRCODE  *strcode;

    if (pstrcode == NULL) {
        L_WARNING("ptr address is null!\n", __func__);
        return;
    }

    if ((strcode = *pstrcode) == NULL)
        return;

    sarrayDestroy(&strcode->function);
    sarrayDestroy(&strcode->data);
    sarrayDestroy(&strcode->descr);
    LEPT_FREE(strcode);
    *pstrcode = NULL;
}


/*!
 * \brief   strcodeCreateFromFile()
 *
 * \param[in]    filein    containing filenames of serialized data
 * \param[in]    fileno    integer that labels the two output files
 * \param[in]    outdir    [optional] if null, files are made in /tmp/lept/auto
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The %filein has one filename on each line.
 *          Comment lines begin with "#".
 *      (2) The output is 2 files:
 *             autogen.[fileno].c
 *             autogen.[fileno].h
 * </pre>
 */
l_ok
strcodeCreateFromFile(const char  *filein,
                      l_int32      fileno,
                      const char  *outdir)
{
char        *fname;
const char  *type;
l_uint8     *data;
size_t       nbytes;
l_int32      i, n, index;
SARRAY      *sa;
L_STRCODE   *strcode;

    if (!filein)
        return ERROR_INT("filein not defined", __func__, 1);

    if ((data = l_binaryRead(filein, &nbytes)) == NULL)
        return ERROR_INT("data not read from file", __func__, 1);
    sa = sarrayCreateLinesFromString((char *)data, 0);
    LEPT_FREE(data);
    if (!sa)
        return ERROR_INT("sa not made", __func__, 1);
    if ((n = sarrayGetCount(sa)) == 0) {
        sarrayDestroy(&sa);
        return ERROR_INT("no filenames in the file", __func__, 1);
    }

    strcode = strcodeCreate(fileno);

    for (i = 0; i < n; i++) {
        fname = sarrayGetString(sa, i, L_NOCOPY);
        if (fname[0] == '#') continue;
        if (l_getIndexFromFile(fname, &index)) {
            L_ERROR("File %s has no recognizable type\n", __func__, fname);
        } else {
            type = l_assoc[index].type;
            L_INFO("File %s is type %s\n", __func__, fname, type);
            strcodeGenerate(strcode, fname, type);
        }
    }
    strcodeFinalize(&strcode, outdir);
    sarrayDestroy(&sa);
    return 0;
}


/*!
 * \brief   strcodeGenerate()
 *
 * \param[in]    strcode    for accumulating data
 * \param[in]    filein     input file with serialized data
 * \param[in]    type       of data; use the typedef string
 * \return  0 if OK, 1 on error.
 *
 * <pre>
 * Notes:
 *      (1) The generated function name is
 *            l_autodecode_[fileno]()
 *          where [fileno] is the index label for the pair of output files.
 *      (2) To deserialize this data, the function is called with the
 *          argument 'ifunc', which increments each time strcodeGenerate()
 *          is called.
 * </pre>
 */
l_ok
strcodeGenerate(L_STRCODE   *strcode,
                const char  *filein,
                const char  *type)
{
char    *strdata, *strfunc, *strdescr;
l_int32  itype;

    if (!strcode)
        return ERROR_INT("strcode not defined", __func__, 1);
    if (!filein)
        return ERROR_INT("filein not defined", __func__, 1);
    if (!type)
        return ERROR_INT("type not defined", __func__, 1);

        /* Get the index corresponding to type and validate */
    if (l_getIndexFromType(type, &itype) == 1)
        return ERROR_INT("data type unknown", __func__, 1);

        /* Generate the encoded data string */
    if ((strdata = l_genDataString(filein, strcode->ifunc)) == NULL)
        return ERROR_INT("strdata not made", __func__, 1);
    sarrayAddString(strcode->data, strdata, L_INSERT);

        /* Generate the case data for the decoding function */
    strfunc = l_genCaseString(strcode->ifunc, itype);
    sarrayAddString(strcode->function, strfunc, L_INSERT);

        /* Generate row of table for function type selection */
    strdescr = l_genDescrString(filein, strcode->ifunc, itype);
    sarrayAddString(strcode->descr, strdescr, L_INSERT);

    strcode->n++;
    strcode->ifunc++;
    return 0;
}


/*!
 * \brief   strcodeFinalize()
 *
 * \param[in,out]  pstrcode   destroys and sets to null after .c and .h files
 *                            have been generated
 * \param[in]      outdir     [optional] if NULL, make files in /tmp/lept/auto
 * \return     0 if OK; 1 on error
 */
l_int32
strcodeFinalize(L_STRCODE  **pstrcode,
                const char  *outdir)
{
char        buf[256];
char       *filestr, *casestr, *descr, *datastr, *realoutdir;
l_int32     actstart, end, newstart, fileno, nbytes;
size_t      size;
L_STRCODE  *strcode;
SARRAY     *sa1, *sa2, *sa3;

    lept_mkdir("lept/auto");

    if (!pstrcode || *pstrcode == NULL)
        return ERROR_INT("No input data", __func__, 1);
    strcode = *pstrcode;
    if (!outdir) {
        L_INFO("no outdir specified; writing to /tmp/lept/auto\n", __func__);
        realoutdir = stringNew("/tmp/lept/auto");
    } else {
        realoutdir = stringNew(outdir);
    }

    /* ------------------------------------------------------- */
    /*              Make the output autogen.*.c file           */
    /* ------------------------------------------------------- */

       /* Make array of textlines from TEMPLATE1 */
    filestr = (char *)l_binaryRead(TEMPLATE1, &size);
    sa1 = sarrayCreateLinesFromString(filestr, 1);
    LEPT_FREE(filestr);
    sa3 = sarrayCreate(0);

        /* Copyright notice */
    sarrayParseRange(sa1, 0, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa1, actstart, end);

        /* File name comment */
    fileno = strcode->fileno;
    snprintf(buf, sizeof(buf), " *   autogen.%d.c", fileno);
    sarrayAddString(sa3, buf, L_COPY);

        /* More text */
    sarrayParseRange(sa1, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa1, actstart, end);

        /* Description of function types by index */
    descr = sarrayToString(strcode->descr, 1);
    descr[strlen(descr) - 1] = '\0';
    sarrayAddString(sa3, descr, L_INSERT);

        /* Includes */
    sarrayParseRange(sa1, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa1, actstart, end);
    snprintf(buf, sizeof(buf), "#include \"autogen.%d.h\"", fileno);
    sarrayAddString(sa3, buf, L_COPY);

        /* Header for auto-generated deserializers */
    sarrayParseRange(sa1, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa1, actstart, end);

        /* Function name (as comment) */
    snprintf(buf, sizeof(buf), " * \\brief  l_autodecode_%d()", fileno);
    sarrayAddString(sa3, buf, L_COPY);

        /* Input and return values */
    sarrayParseRange(sa1, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa1, actstart, end);

        /* Function name */
    snprintf(buf, sizeof(buf), "l_autodecode_%d(l_int32 index)", fileno);
    sarrayAddString(sa3, buf, L_COPY);

        /* Stack vars */
    sarrayParseRange(sa1, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa1, actstart, end);

        /* Declaration of nfunc on stack */
    snprintf(buf, sizeof(buf), "l_int32   nfunc = %d;\n", strcode->n);
    sarrayAddString(sa3, buf, L_COPY);

        /* Declaration of PROCNAME */
    snprintf(buf, sizeof(buf), "    PROCNAME(\"l_autodecode_%d\");", fileno);
    sarrayAddString(sa3, buf, L_COPY);

        /* Test input variables */
    sarrayParseRange(sa1, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa1, actstart, end);

        /* Insert case string */
    casestr = sarrayToString(strcode->function, 0);
    casestr[strlen(casestr) - 1] = '\0';
    sarrayAddString(sa3, casestr, L_INSERT);

        /* End of function */
    sarrayParseRange(sa1, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa1, actstart, end);

        /* Flatten to string and output to autogen*.c file */
    filestr = sarrayToString(sa3, 1);
    nbytes = strlen(filestr);
    snprintf(buf, sizeof(buf), "%s/autogen.%d.c", realoutdir, fileno);
    l_binaryWrite(buf, "w", filestr, nbytes);
    LEPT_FREE(filestr);
    sarrayDestroy(&sa1);
    sarrayDestroy(&sa3);

    /* ------------------------------------------------------- */
    /*              Make the output autogen.*.h file           */
    /* ------------------------------------------------------- */

       /* Make array of textlines from TEMPLATE2 */
    filestr = (char *)l_binaryRead(TEMPLATE2, &size);
    sa2 = sarrayCreateLinesFromString(filestr, 1);
    LEPT_FREE(filestr);
    sa3 = sarrayCreate(0);

        /* Copyright notice */
    sarrayParseRange(sa2, 0, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);

        /* File name comment */
    snprintf(buf, sizeof(buf), " *   autogen.%d.h", fileno);
    sarrayAddString(sa3, buf, L_COPY);

        /* More text */
    sarrayParseRange(sa2, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);

        /* Beginning header protection */
    snprintf(buf, sizeof(buf), "#ifndef  LEPTONICA_AUTOGEN_%d_H\n"
                               "#define  LEPTONICA_AUTOGEN_%d_H",
             fileno, fileno);
    sarrayAddString(sa3, buf, L_COPY);

        /* Prototype header text */
    sarrayParseRange(sa2, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);

        /* Prototype declaration */
    snprintf(buf, sizeof(buf), "void *l_autodecode_%d(l_int32 index);", fileno);
    sarrayAddString(sa3, buf, L_COPY);

        /* Prototype trailer text */
    sarrayParseRange(sa2, newstart, &actstart, &end, &newstart, "--", 0);
    sarrayAppendRange(sa3, sa2, actstart, end);

        /* Insert serialized data strings */
    datastr = sarrayToString(strcode->data, 1);
    datastr[strlen(datastr) - 1] = '\0';
    sarrayAddString(sa3, datastr, L_INSERT);

        /* End header protection */
    snprintf(buf, sizeof(buf), "#endif  /* LEPTONICA_AUTOGEN_%d_H */", fileno);
    sarrayAddString(sa3, buf, L_COPY);

        /* Flatten to string and output to autogen*.h file */
    filestr = sarrayToString(sa3, 1);
    nbytes = strlen(filestr);
    snprintf(buf, sizeof(buf), "%s/autogen.%d.h", realoutdir, fileno);
    l_binaryWrite(buf, "w", filestr, nbytes);
    LEPT_FREE(filestr);
    LEPT_FREE(realoutdir);
    sarrayDestroy(&sa2);
    sarrayDestroy(&sa3);

        /* Cleanup */
    strcodeDestroy(pstrcode);
    return 0;
}


/*!
 * \brief   l_getStructStrFromFile()
 *
 * \param[in]    filename
 * \param[in]    field   (L_STR_TYPE, L_STR_NAME, L_STR_READER, L_STR_MEMREADER)
 * \param[out]   pstr    struct string for this file
 * \return  0 if found, 1 on error.
 *
 * <pre>
 * Notes:
 *      (1) For example, if %field == L_STR_NAME, and the file is a serialized
 *          pixa, this will return "Pixa", the name of the struct.
 *      (2) Caller must free the returned string.
 * </pre>
 */
l_int32
l_getStructStrFromFile(const char  *filename,
                       l_int32      field,
                       char       **pstr)
{
l_int32  index;

    if (!pstr)
        return ERROR_INT("&str not defined", __func__, 1);
    *pstr = NULL;
    if (!filename)
        return ERROR_INT("filename not defined", __func__, 1);
    if (field != L_STR_TYPE && field != L_STR_NAME &&
        field != L_STR_READER && field != L_STR_MEMREADER)
        return ERROR_INT("invalid field", __func__, 1);

    if (l_getIndexFromFile(filename, &index))
        return ERROR_INT("index not retrieved", __func__, 1);
    if (field == L_STR_TYPE)
        *pstr = stringNew(l_assoc[index].type);
    else if (field == L_STR_NAME)
        *pstr = stringNew(l_assoc[index].structname);
    else if (field == L_STR_READER)
        *pstr = stringNew(l_assoc[index].reader);
    else   /* field == L_STR_MEMREADER */
        *pstr = stringNew(l_assoc[index].memreader);
    return 0;
}


/*---------------------------------------------------------------------*/
/*                           Static helpers                            */
/*---------------------------------------------------------------------*/
/*!
 * \brief   l_getIndexFromType()
 *
 * \param[in]    type     e.g., "PIXA"
 * \param[out]   pindex   found index
 * \return  0 if found, 1 if not.
 *
 * <pre>
 * Notes:
 *      (1) For valid type, %found == true and %index > 0.
 * </pre>
 */
static l_int32
l_getIndexFromType(const char  *type,
                   l_int32     *pindex)
{
l_int32  i, found;

    if (!pindex)
        return ERROR_INT("&index not defined", __func__, 1);
    *pindex = 0;
    if (!type)
        return ERROR_INT("type string not defined", __func__, 1);

    found = 0;
    for (i = 1; i <= l_ntypes; i++) {
        if (strcmp(type, l_assoc[i].type) == 0) {
            found = 1;
            *pindex = i;
            break;
        }
    }
    return !found;
}


/*!
 * \brief   l_getIndexFromStructname()
 *
 * \param[in]    sn       structname e.g., "Pixa"
 * \param[out]   pindex   found index
 * \return  0 if found, 1 if not.
 *
 * <pre>
 * Notes:
 *      (1) This is used to identify the type of serialized file;
 *          the first word in the file is the structname.
 *      (2) For valid structname, %found == true and %index > 0.
 * </pre>
 */
static l_int32
l_getIndexFromStructname(const char  *sn,
                         l_int32     *pindex)
{
l_int32  i, found;

    if (!pindex)
        return ERROR_INT("&index not defined", __func__, 1);
    *pindex = 0;
    if (!sn)
        return ERROR_INT("sn string not defined", __func__, 1);

    found = 0;
    for (i = 1; i <= l_ntypes; i++) {
        if (strcmp(sn, l_assoc[i].structname) == 0) {
            found = 1;
            *pindex = i;
            break;
        }
    }
    return !found;
}


/*!
 * \brief   l_getIndexFromFile()
 *
 * \param[in]    filename
 * \param[out]   pindex     found index
 * \return  0 if found, 1 on error.
 */
static l_int32
l_getIndexFromFile(const char  *filename,
                   l_int32     *pindex)
{
char     buf[256];
char    *word;
FILE    *fp;
l_int32  notfound, format;
SARRAY  *sa;

    if (!pindex)
        return ERROR_INT("&index not defined", __func__, 1);
    *pindex = 0;
    if (!filename)
        return ERROR_INT("filename not defined", __func__, 1);

        /* Open the stream, read lines until you find one with more
         * than a newline, and grab the first word. */
    if ((fp = fopenReadStream(filename)) == NULL)
        return ERROR_INT_1("stream not opened", filename, __func__, 1);
    do {
        if ((fgets(buf, sizeof(buf), fp)) == NULL) {
            fclose(fp);
            return ERROR_INT_1("fgets read fail", filename, __func__, 1);
        }
    } while (buf[0] == '\n');
    fclose(fp);
    sa = sarrayCreateWordsFromString(buf);
    word = sarrayGetString(sa, 0, L_NOCOPY);

        /* Find the index associated with the word.  If it is not
         * found, test to see if the file is a compressed pix. */
    notfound = l_getIndexFromStructname(word, pindex);
    sarrayDestroy(&sa);
    if (notfound) {  /* maybe a Pix */
        if (findFileFormat(filename, &format) == 0) {
            l_getIndexFromStructname("Pix", pindex);
        } else {
            return ERROR_INT_1("no file type identified",
                               filename, __func__, 1);
        }
    }

    return 0;
}


/*!
 * \brief   l_genDataString()
 *
 * \param[in]    filein   input file of serialized data
 * \param[in]    ifunc    index into set of functions in output file
 * \return  encoded ascii data string, or NULL on error reading from file
 */
static char *
l_genDataString(const char  *filein,
                l_int32      ifunc)
{
char      buf[80];
char     *cdata1, *cdata2, *cdata3;
l_uint8  *data1, *data2;
l_int32   csize1, csize2;
size_t    size1, size2;
SARRAY   *sa;

    if (!filein)
        return (char *)ERROR_PTR("filein not defined", __func__, NULL);

        /* Read it in, gzip it, encode, and reformat.  We gzip because some
         * serialized data has a significant amount of ascii content. */
    if ((data1 = l_binaryRead(filein, &size1)) == NULL)
        return (char *)ERROR_PTR("bindata not returned", __func__, NULL);
    data2 = zlibCompress(data1, size1, &size2);
    cdata1 = encodeBase64(data2, size2, &csize1);
    cdata2 = reformatPacked64(cdata1, csize1, 4, 72, 1, &csize2);
    LEPT_FREE(data1);
    LEPT_FREE(data2);
    LEPT_FREE(cdata1);

        /* Prepend the string declaration signature and put it together */
    sa = sarrayCreate(3);
    snprintf(buf, sizeof(buf), "static const char *l_strdata_%d =\n", ifunc);
    sarrayAddString(sa, buf, L_COPY);
    sarrayAddString(sa, cdata2, L_INSERT);
    sarrayAddString(sa, ";\n", L_COPY);
    cdata3 = sarrayToString(sa, 0);
    sarrayDestroy(&sa);
    return cdata3;
}


/*!
 * \brief   l_genCaseString()
 *
 * \param[in]    ifunc   index into set of functions in generated file
 * \param[in]    itype   index into type of function to be used
 * \return  case string for this decoding function
 *
 * <pre>
 * Notes:
 *      (1) %ifunc and %itype have been validated, so no error can occur
 * </pre>
 */
static char *
l_genCaseString(l_int32  ifunc,
                l_int32  itype)
{
char   buf[256];
char  *code = NULL;

    snprintf(buf, sizeof(buf), "    case %d:\n", ifunc);
    stringJoinIP(&code, buf);
    snprintf(buf, sizeof(buf),
        "        data1 = decodeBase64(l_strdata_%d, strlen(l_strdata_%d), "
        "&size1);\n", ifunc, ifunc);
    stringJoinIP(&code, buf);
    stringJoinIP(&code,
                 "        data2 = zlibUncompress(data1, size1, &size2);\n");
    snprintf(buf, sizeof(buf),
             "        result = (void *)%s(data2, size2);\n",
             l_assoc[itype].memreader);
    stringJoinIP(&code, buf);
    stringJoinIP(&code, "        lept_free(data1);\n");
    stringJoinIP(&code, "        lept_free(data2);\n");
    stringJoinIP(&code, "        break;\n");
    return code;
}


/*!
 * \brief   l_genDescrString()
 *
 * \param[in]    filein   input file of serialized data
 * \param[in]    ifunc    index into set of functions in generated file
 * \param[in]    itype    index into type of function to be used
 * \return  description string for this decoding function
 */
static char *
l_genDescrString(const char  *filein,
                 l_int32      ifunc,
                 l_int32      itype)
{
char   buf[256];
char  *tail;

    if (!filein)
        return (char *)ERROR_PTR("filein not defined", __func__, NULL);

    splitPathAtDirectory(filein, NULL, &tail);
    snprintf(buf, sizeof(buf), " *     %-2d       %-10s    %-14s   %s",
             ifunc, l_assoc[itype].type, l_assoc[itype].reader, tail);

    LEPT_FREE(tail);
    return stringNew(buf);
}
