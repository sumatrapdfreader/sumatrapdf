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
 * \file  sarray1.c
 * <pre>
 *
 *      Create/Destroy/Copy
 *          SARRAY    *sarrayCreate()
 *          SARRAY    *sarrayCreateInitialized()
 *          SARRAY    *sarrayCreateWordsFromString()
 *          SARRAY    *sarrayCreateLinesFromString()
 *          void      *sarrayDestroy()
 *          SARRAY    *sarrayCopy()
 *          SARRAY    *sarrayClone()
 *
 *      Add/Remove string
 *          l_int32    sarrayAddString()
 *          static l_int32  sarrayExtendArray()
 *          char      *sarrayRemoveString()
 *          l_int32    sarrayReplaceString()
 *          l_int32    sarrayClear()
 *
 *      Accessors
 *          l_int32    sarrayGetCount()
 *          char     **sarrayGetArray()
 *          char      *sarrayGetString()
 *
 *      Conversion back to string
 *          char      *sarrayToString()
 *          char      *sarrayToStringRange()
 *
 *      Concatenate strings uniformly within the sarray
 *          SARRAY    *sarrayConcatUniformly()
 *
 *      Join 2 sarrays
 *          l_int32    sarrayJoin()
 *          l_int32    sarrayAppendRange()
 *
 *      Pad an sarray to be the same size as another sarray
 *          l_int32    sarrayPadToSameSize()
 *
 *      Convert word sarray to (formatted) line sarray
 *          SARRAY    *sarrayConvertWordsToLines()
 *
 *      Split string on separator list
 *          SARRAY    *sarraySplitString()
 *
 *      Filter sarray
 *          SARRAY    *sarraySelectBySubstring()
 *          SARRAY    *sarraySelectRange()
 *          l_int32    sarrayParseRange()
 *
 *      Serialize for I/O
 *          SARRAY    *sarrayRead()
 *          SARRAY    *sarrayReadStream()
 *          SARRAY    *sarrayReadMem()
 *          l_int32    sarrayWrite()
 *          l_int32    sarrayWriteStream()
 *          l_int32    sarrayWriteStderr()
 *          l_int32    sarrayWriteMem()
 *          l_int32    sarrayAppend()
 *
 *      Directory filenames
 *          SARRAY    *getNumberedPathnamesInDirectory()
 *          SARRAY    *getSortedPathnamesInDirectory()
 *          SARRAY    *convertSortedToNumberedPathnames()
 *          SARRAY    *getFilenamesInDirectory()
 *
 *      These functions are important for efficient manipulation
 *      of string data, and they have found widespread use in
 *      leptonica.  For example:
 *         (1) to generate text files: e.g., PostScript and PDF
 *             wrappers around sets of images
 *         (2) to parse text files: e.g., extracting prototypes
 *             from the source to generate allheaders.h
 *         (3) to generate code for compilation: e.g., the fast
 *             dwa code for arbitrary structuring elements.
 *
 *      Comments on usage:
 *
 *          The user is responsible for correctly disposing of strings
 *          that have been extracted from sarrays.  In the following,
 *          "str_not_owned" means the returned handle does not own the string,
 *          and "str_owned" means the returned handle owns the string.
 *            - To extract a string from an Sarray in order to inspect it
 *              or to make a copy of it later, get a handle to it:
 *                  copyflag = L_NOCOPY.
 *              In this case, you must neither free the string nor put it
 *              directly in another array:
 *                 str-not-owned = sarrayGetString(sa, index, L_NOCOPY);
 *            - To extract a copy of a string from an Sarray, use:
 *                 str-owned = sarrayGetString(sa, index, L_COPY);
 *            ~ To insert a string that is in one array into another
 *              array (always leaving the first array intact), there are
 *              two options:
 *                 (1) use copyflag = L_COPY to make an immediate copy,
 *                     which you then add to the second array by insertion:
 *                       str-owned = sarrayGetString(sa, index, L_COPY);
 *                       sarrayAddString(sa, str-owned, L_INSERT);
 *                 (2) use copyflag = L_NOCOPY to get another handle to
 *                     the string; you then add a copy of it to the
 *                     second string array:
 *                       str-not-owned = sarrayGetString(sa, index, L_NOCOPY);
 *                       sarrayAddString(sa, str-not-owned, L_COPY).
 *              sarrayAddString() transfers ownership to the Sarray, so never
 *              use L_INSERT if the string is owned by another array.
 *
 *              In all cases, when you use copyflag = L_COPY to extract
 *              a string from an array, you must either free it
 *              or insert it in an array that will be freed later.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#ifndef _WIN32
#include <dirent.h>     /* unix only */
#include <sys/stat.h>
#include <limits.h>  /* needed for realpath() */
#include <stdlib.h>  /* needed for realpath() */
#endif  /* ! _WIN32 */
#include "allheaders.h"
#include "array_internal.h"

static const l_uint32  MaxPtrArraySize = 50000000;    /* 50 million */
static const l_int32   InitialPtrArraySize = 50;      /*!< n'importe quoi */

    /* Static functions */
static l_int32 sarrayExtendArray(SARRAY *sa);


/*--------------------------------------------------------------------------*
 *                   String array create/destroy/copy/extend                *
 *--------------------------------------------------------------------------*/
/*!
 * \brief   sarrayCreate()
 *
 * \param[in]    n    size of string ptr array to be alloc'd; use 0 for default
 * \return  sarray, or NULL on error
 */
SARRAY *
sarrayCreate(l_int32  n)
{
SARRAY  *sa;

    if (n <= 0 || n > (l_int32)MaxPtrArraySize)
        n = InitialPtrArraySize;

    sa = (SARRAY *)LEPT_CALLOC(1, sizeof(SARRAY));
    if ((sa->array = (char **)LEPT_CALLOC(n, sizeof(char *))) == NULL) {
        sarrayDestroy(&sa);
        return (SARRAY *)ERROR_PTR("ptr array not made", __func__, NULL);
    }

    sa->nalloc = n;
    sa->n = 0;
    sa->refcount = 1;
    return sa;
}


/*!
 * \brief   sarrayCreateInitialized()
 *
 * \param[in]    n         size of string ptr array to be alloc'd
 * \param[in]    initstr   string to be initialized on the full array
 * \return  sarray, or NULL on error
 */
SARRAY *
sarrayCreateInitialized(l_int32      n,
                        const char  *initstr)
{
l_int32  i;
SARRAY  *sa;

    if (n <= 0)
        return (SARRAY *)ERROR_PTR("n must be > 0", __func__, NULL);
    if (!initstr)
        return (SARRAY *)ERROR_PTR("initstr not defined", __func__, NULL);

    sa = sarrayCreate(n);
    for (i = 0; i < n; i++)
        sarrayAddString(sa, initstr, L_COPY);
    return sa;
}


/*!
 * \brief   sarrayCreateWordsFromString()
 *
 * \param[in]    string
 * \return  sarray, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This finds the number of word substrings, creates an sarray
 *          of this size, and puts copies of each substring into the sarray.
 * </pre>
 */
SARRAY *
sarrayCreateWordsFromString(const char  *string)
{
char     separators[] = " \n\t";
l_int32  i, nsub, size, inword;
SARRAY  *sa;

    if (!string)
        return (SARRAY *)ERROR_PTR("textstr not defined", __func__, NULL);

        /* Find the number of words */
    size = strlen(string);
    nsub = 0;
    inword = FALSE;
    for (i = 0; i < size; i++) {
        if (inword == FALSE &&
           (string[i] != ' ' && string[i] != '\t' && string[i] != '\n')) {
           inword = TRUE;
           nsub++;
        } else if (inword == TRUE &&
           (string[i] == ' ' || string[i] == '\t' || string[i] == '\n')) {
           inword = FALSE;
        }
    }

    if ((sa = sarrayCreate(nsub)) == NULL)
        return (SARRAY *)ERROR_PTR("sa not made", __func__, NULL);
    sarraySplitString(sa, string, separators);

    return sa;
}


/*!
 * \brief   sarrayCreateLinesFromString()
 *
 * \param[in]    string
 * \param[in]    blankflag    0 to exclude blank lines; 1 to include
 * \return  sarray, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This finds the number of line substrings, each of which
 *          ends with a newline, and puts a copy of each substring
 *          in a new sarray.
 *      (2) The newline characters are removed from each substring.
 * </pre>
 */
SARRAY *
sarrayCreateLinesFromString(const char  *string,
                            l_int32      blankflag)
{
l_int32  i, nsub, size, startptr;
char    *cstring, *substring;
SARRAY  *sa;

    if (!string)
        return (SARRAY *)ERROR_PTR("textstr not defined", __func__, NULL);

        /* Find the number of lines */
    size = strlen(string);
    nsub = 0;
    for (i = 0; i < size; i++) {
        if (string[i] == '\n')
            nsub++;
    }

    if ((sa = sarrayCreate(nsub)) == NULL)
        return (SARRAY *)ERROR_PTR("sa not made", __func__, NULL);

    if (blankflag) {  /* keep blank lines as null strings */
            /* Make a copy for munging */
        if ((cstring = stringNew(string)) == NULL) {
            sarrayDestroy(&sa);
            return (SARRAY *)ERROR_PTR("cstring not made", __func__, NULL);
        }
            /* We'll insert nulls like strtok */
        startptr = 0;
        for (i = 0; i < size; i++) {
            if (cstring[i] == '\n') {
                cstring[i] = '\0';
                if (i > 0 && cstring[i - 1] == '\r')
                    cstring[i - 1] = '\0';  /* also remove Windows CR */
                if ((substring = stringNew(cstring + startptr)) == NULL) {
                    sarrayDestroy(&sa);
                    LEPT_FREE(cstring);
                    return (SARRAY *)ERROR_PTR("substring not made",
                                                __func__, NULL);
                }
                sarrayAddString(sa, substring, L_INSERT);
/*                lept_stderr("substring = %s\n", substring); */
                startptr = i + 1;
            }
        }
        if (startptr < size) {  /* no newline at end of last line */
            if ((substring = stringNew(cstring + startptr)) == NULL) {
                sarrayDestroy(&sa);
                LEPT_FREE(cstring);
                return (SARRAY *)ERROR_PTR("substring not made",
                                           __func__, NULL);
            }
            sarrayAddString(sa, substring, L_INSERT);
/*            lept_stderr("substring = %s\n", substring); */
        }
        LEPT_FREE(cstring);
    } else {  /* remove blank lines; use strtok */
        sarraySplitString(sa, string, "\r\n");
    }

    return sa;
}


/*!
 * \brief   sarrayDestroy()
 *
 * \param[in,out]   psa    will be set to null before returning
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) Decrements the ref count and, if 0, destroys the sarray.
 *      (2) Always nulls the input ptr.
 * </pre>
 */
void
sarrayDestroy(SARRAY  **psa)
{
l_int32  i;
SARRAY  *sa;

    if (psa == NULL) {
        L_WARNING("ptr address is NULL!\n", __func__);
        return;
    }
    if ((sa = *psa) == NULL)
        return;

    if (--sa->refcount == 0) {
        if (sa->array) {
            for (i = 0; i < sa->n; i++) {
                if (sa->array[i])
                    LEPT_FREE(sa->array[i]);
            }
            LEPT_FREE(sa->array);
        }
        LEPT_FREE(sa);
    }
    *psa = NULL;
}


/*!
 * \brief   sarrayCopy()
 *
 * \param[in]    sa    string array
 * \return  copy of sarray, or NULL on error
 */
SARRAY *
sarrayCopy(SARRAY  *sa)
{
l_int32  i;
SARRAY  *csa;

    if (!sa)
        return (SARRAY *)ERROR_PTR("sa not defined", __func__, NULL);

    if ((csa = sarrayCreate(sa->nalloc)) == NULL)
        return (SARRAY *)ERROR_PTR("csa not made", __func__, NULL);

    for (i = 0; i < sa->n; i++)
        sarrayAddString(csa, sa->array[i], L_COPY);

    return csa;
}


/*!
 * \brief   sarrayClone()
 *
 * \param[in]    sa    string array
 * \return  ptr to same sarray, or NULL on error
 */
SARRAY *
sarrayClone(SARRAY  *sa)
{
    if (!sa)
        return (SARRAY *)ERROR_PTR("sa not defined", __func__, NULL);
    ++sa->refcount;
    return sa;
}


/*!
 * \brief   sarrayAddString()
 *
 * \param[in]    sa         string array
 * \param[in]    string     string to be added
 * \param[in]    copyflag   L_INSERT, L_NOCOPY or L_COPY
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) See usage comments at the top of this file.  L_INSERT is
 *          equivalent to L_NOCOPY.
 * </pre>
 */
l_ok
sarrayAddString(SARRAY      *sa,
                const char  *string,
                l_int32      copyflag)
{
l_int32  n;

    if (!sa)
        return ERROR_INT("sa not defined", __func__, 1);
    if (!string)
        return ERROR_INT("string not defined", __func__, 1);
    if (copyflag != L_INSERT && copyflag != L_NOCOPY && copyflag != L_COPY)
        return ERROR_INT("invalid copyflag", __func__, 1);

    n = sarrayGetCount(sa);
    if (n >= sa->nalloc) {
        if (sarrayExtendArray(sa))
            return ERROR_INT("extension failed", __func__, 1);
    }

    if (copyflag == L_COPY)
        sa->array[n] = stringNew(string);
    else  /* L_INSERT or L_NOCOPY */
        sa->array[n] = (char *)string;
    sa->n++;
    return 0;
}


/*!
 * \brief   sarrayExtendArray()
 *
 * \param[in]    sa    string array
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Doubles the size of the string ptr array.
 *      (2) The max number of strings is 50M.
 * </pre>
 */
static l_int32
sarrayExtendArray(SARRAY  *sa)
{
size_t  oldsize, newsize;

    if (!sa)
        return ERROR_INT("sa not defined", __func__, 1);
    if (sa->nalloc >= (l_int32)MaxPtrArraySize)  /* belt & suspenders */
        return ERROR_INT("sa at maximum ptr size; can't extend", __func__, 1);
    oldsize = sa->nalloc * sizeof(char *);
    if (sa->nalloc > (l_int32)(MaxPtrArraySize / 2)) {
        newsize = MaxPtrArraySize * sizeof(char *);
        sa->nalloc = (l_int32)MaxPtrArraySize;
    } else {
        newsize = 2 * oldsize;
        sa->nalloc *= 2;
    }
    if ((sa->array = (char **)reallocNew((void **)&sa->array,
                                         oldsize, newsize)) == NULL)
        return ERROR_INT("new ptr array not returned", __func__, 1);

    return 0;
}


/*!
 * \brief   sarrayRemoveString()
 *
 * \param[in]    sa       string array
 * \param[in]    index    of string within sarray
 * \return  removed string, or NULL on error
 */
char *
sarrayRemoveString(SARRAY  *sa,
                   l_int32  index)
{
char    *string;
char   **array;
l_int32  i, n, nalloc;

    if (!sa)
        return (char *)ERROR_PTR("sa not defined", __func__, NULL);

    if ((array = sarrayGetArray(sa, &nalloc, &n)) == NULL)
        return (char *)ERROR_PTR("array not returned", __func__, NULL);

    if (index < 0 || index >= n)
        return (char *)ERROR_PTR("array index out of bounds", __func__, NULL);

    string = array[index];

        /* If removed string is not at end of array, shift
         * to fill in, maintaining original ordering.
         * Note: if we didn't care about the order, we could
         * put the last string array[n - 1] directly into the hole.  */
    for (i = index; i < n - 1; i++)
        array[i] = array[i + 1];

    sa->n--;
    return string;
}


/*!
 * \brief   sarrayReplaceString()
 *
 * \param[in]    sa         string array
 * \param[in]    index      of string within sarray to be replaced
 * \param[in]    newstr     string to replace existing one
 * \param[in]    copyflag   L_INSERT, L_COPY
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This destroys an existing string and replaces it with
 *          the new string or a copy of it.
 *      (2) By design, an sarray is always compacted, so there are
 *          never any holes (null ptrs) in the ptr array up to the
 *          current count.
 * </pre>
 */
l_ok
sarrayReplaceString(SARRAY  *sa,
                    l_int32  index,
                    char    *newstr,
                    l_int32  copyflag)
{
char    *str;
l_int32  n;

    if (!sa)
        return ERROR_INT("sa not defined", __func__, 1);
    n = sarrayGetCount(sa);
    if (index < 0 || index >= n)
        return ERROR_INT("array index out of bounds", __func__, 1);
    if (!newstr)
        return ERROR_INT("newstr not defined", __func__, 1);
    if (copyflag != L_INSERT && copyflag != L_COPY)
        return ERROR_INT("invalid copyflag", __func__, 1);

    LEPT_FREE(sa->array[index]);
    if (copyflag == L_INSERT)
        str = newstr;
    else  /* L_COPY */
        str = stringNew(newstr);
    sa->array[index] = str;
    return 0;
}


/*!
 * \brief   sarrayClear()
 *
 * \param[in]    sa    string array
 * \return  0 if OK; 1 on error
 */
l_ok
sarrayClear(SARRAY  *sa)
{
l_int32  i;

    if (!sa)
        return ERROR_INT("sa not defined", __func__, 1);
    for (i = 0; i < sa->n; i++) {  /* free strings and null ptrs */
        LEPT_FREE(sa->array[i]);
        sa->array[i] = NULL;
    }
    sa->n = 0;
    return 0;
}


/*----------------------------------------------------------------------*
 *                               Accessors                              *
 *----------------------------------------------------------------------*/
/*!
 * \brief   sarrayGetCount()
 *
 * \param[in]    sa    string array
 * \return  count, or 0 if no strings or on error
 */
l_int32
sarrayGetCount(SARRAY  *sa)
{
    if (!sa)
        return ERROR_INT("sa not defined", __func__, 0);
    return sa->n;
}


/*!
 * \brief   sarrayGetArray()
 *
 * \param[in]    sa        string array
 * \param[out]   pnalloc   [optional] number allocated string ptrs
 * \param[out]   pn        [optional] number allocated strings
 * \return  ptr to string array, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Caution: the returned array is not a copy, so caller
 *          must not destroy it!
 * </pre>
 */
char **
sarrayGetArray(SARRAY   *sa,
               l_int32  *pnalloc,
               l_int32  *pn)
{
char  **array;

    if (!sa)
        return (char **)ERROR_PTR("sa not defined", __func__, NULL);

    array = sa->array;
    if (pnalloc) *pnalloc = sa->nalloc;
    if (pn) *pn = sa->n;

    return array;
}


/*!
 * \brief   sarrayGetString()
 *
 * \param[in]    sa         string array
 * \param[in]    index      to the index-th string
 * \param[in]    copyflag   L_NOCOPY or L_COPY
 * \return  string, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) See usage comments at the top of this file.
 *      (2) To get a pointer to the string itself, use L_NOCOPY.
 *          To get a copy of the string, use L_COPY.
 * </pre>
 */
char *
sarrayGetString(SARRAY  *sa,
                l_int32  index,
                l_int32  copyflag)
{
    if (!sa)
        return (char *)ERROR_PTR("sa not defined", __func__, NULL);
    if (index < 0 || index >= sa->n)
        return (char *)ERROR_PTR("index not valid", __func__, NULL);
    if (copyflag != L_NOCOPY && copyflag != L_COPY)
        return (char *)ERROR_PTR("invalid copyflag", __func__, NULL);

    if (copyflag == L_NOCOPY)
        return sa->array[index];
    else  /* L_COPY */
        return stringNew(sa->array[index]);
}


/*----------------------------------------------------------------------*
 *                      Conversion to string                           *
 *----------------------------------------------------------------------*/
/*!
 * \brief   sarrayToString()
 *
 * \param[in]    sa          string array
 * \param[in]    addnlflag   flag: 0 adds nothing to each substring
 *                                 1 adds '\n' to each substring
 *                                 2 adds ' ' to each substring
 *                                 3 adds ',' to each substring
 * \return  dest string, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Concatenates all the strings in the sarray, preserving
 *          all white space.
 *      (2) If addnlflag != 0, adds '\n', ' ' or ',' after each substring.
 *      (3) This function was NOT implemented as:
 *            for (i = 0; i < n; i++)
 *                strcat(dest, sarrayGetString(sa, i, L_NOCOPY));
 *          Do you see why?
 * </pre>
 */
char *
sarrayToString(SARRAY  *sa,
               l_int32  addnlflag)
{
    if (!sa)
        return (char *)ERROR_PTR("sa not defined", __func__, NULL);

    return sarrayToStringRange(sa, 0, 0, addnlflag);
}


/*!
 * \brief   sarrayToStringRange()
 *
 * \param[in]   sa          string array
 * \param[in]   first       index of first string to use; starts with 0
 * \param[in]   nstrings    number of strings to append into the result; use
 *                          0 to append to the end of the sarray
 * \param[in]   addnlflag   flag: 0 adds nothing to each substring
 *                                1 adds '\n' to each substring
 *                                2 adds ' ' to each substring
 *                                3 adds ',' to each substring
 * \return  dest string, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Concatenates the specified strings in the sarray, preserving
 *          all white space.
 *      (2) If addnlflag != 0, adds '\n', ' ' or ',' after each substring.
 *      (3) If the sarray is empty, this returns a string with just
 *          the character corresponding to %addnlflag.
 * </pre>
 */
char *
sarrayToStringRange(SARRAY  *sa,
                    l_int32  first,
                    l_int32  nstrings,
                    l_int32  addnlflag)
{
char    *dest, *src, *str;
l_int32  n, i, last, size, index, len;

    if (!sa)
        return (char *)ERROR_PTR("sa not defined", __func__, NULL);
    if (addnlflag != 0 && addnlflag != 1 && addnlflag != 2 && addnlflag != 3)
        return (char *)ERROR_PTR("invalid addnlflag", __func__, NULL);

    n = sarrayGetCount(sa);

        /* Empty sa; return char corresponding to addnlflag only */
    if (n == 0) {
        if (first == 0) {
            if (addnlflag == 0)
                return stringNew("");
            if (addnlflag == 1)
                return stringNew("\n");
            if (addnlflag == 2)
                return stringNew(" ");
            else  /* addnlflag == 3) */
                return stringNew(",");
        } else {
            return (char *)ERROR_PTR("first not valid", __func__, NULL);
        }
    }

        /* Determine the range of string indices to be used */
    if (first < 0 || first >= n)
        return (char *)ERROR_PTR("first not valid", __func__, NULL);
    if (nstrings == 0 || (nstrings > n - first))
        nstrings = n - first;  /* no overflow */
    last = first + nstrings - 1;

        /* Determine the size of the output string */
    size = 0;
    for (i = first; i <= last; i++) {
        if ((str = sarrayGetString(sa, i, L_NOCOPY)) == NULL)
            return (char *)ERROR_PTR("str not found", __func__, NULL);
        size += strlen(str) + 2;
    }
    if ((dest = (char *)LEPT_CALLOC(size + 1, sizeof(char))) == NULL)
        return (char *)ERROR_PTR("dest not made", __func__, NULL);

        /* Construct the output */
    index = 0;
    for (i = first; i <= last; i++) {
        src = sarrayGetString(sa, i, L_NOCOPY);
        len = strlen(src);
        memcpy(dest + index, src, len);
        index += len;
        if (addnlflag == 1) {
            dest[index] = '\n';
            index++;
        } else if (addnlflag == 2) {
            dest[index] = ' ';
            index++;
        } else if (addnlflag == 3) {
            dest[index] = ',';
            index++;
        }
    }

    return dest;
}


/*----------------------------------------------------------------------*
 *           Concatenate strings uniformly within the sarray            *
 *----------------------------------------------------------------------*/
/*!
 * \brief   sarrayConcatUniformly()
 *
 * \param[in]    sa          string array
 * \param[in]    n           number of strings in output sarray
 * \param[in]    addnlflag   flag: 0 adds nothing to each substring
 *                                 1 adds '\n' to each substring
 *                                 2 adds ' ' to each substring
 *                                 3 adds ',' to each substring
 * \return  dest sarray, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Divides %sa into %n essentially equal sets of strings,
 *          concatenates each set individually, and makes an output
 *          sarray with the %n concatenations.  %n must not exceed the
 *          number of strings in %sa.
 *      (2) If addnlflag != 0, adds '\n', ' ' or ',' after each substring.
 * </pre>
 */
SARRAY *
sarrayConcatUniformly(SARRAY  *sa,
                      l_int32  n,
                      l_int32  addnlflag)
{
l_int32  i, first, ntot, nstr;
char    *str;
NUMA    *na;
SARRAY  *saout;

    if (!sa)
        return (SARRAY *)ERROR_PTR("sa not defined", __func__, NULL);
    ntot = sarrayGetCount(sa);
    if (n < 1)
        return (SARRAY *)ERROR_PTR("n must be >= 1", __func__, NULL);
    if (n > ntot) {
        L_ERROR("n = %d > ntot = %d\n", __func__, n, ntot);
        return NULL;
    }
    if (addnlflag != 0 && addnlflag != 1 && addnlflag != 2 && addnlflag != 3)
        return (SARRAY *)ERROR_PTR("invalid addnlflag", __func__, NULL);

    saout = sarrayCreate(0);
    na = numaGetUniformBinSizes(ntot, n);
    for (i = 0, first = 0; i < n; i++) {
        numaGetIValue(na, i, &nstr);
        str = sarrayToStringRange(sa, first, nstr, addnlflag);
        sarrayAddString(saout, str, L_INSERT);
        first += nstr;
    }
    numaDestroy(&na);
    return saout;
}


/*----------------------------------------------------------------------*
 *                           Join 2 sarrays                             *
 *----------------------------------------------------------------------*/
/*!
 * \brief   sarrayJoin()
 *
 * \param[in]    sa1   to be added to
 * \param[in]    sa2   append to sa1
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Copies of the strings in sarray2 are added to sarray1.
 * </pre>
 */
l_ok
sarrayJoin(SARRAY  *sa1,
           SARRAY  *sa2)
{
char    *str;
l_int32  n, i;

    if (!sa1)
        return ERROR_INT("sa1 not defined", __func__, 1);
    if (!sa2)
        return ERROR_INT("sa2 not defined", __func__, 1);

    n = sarrayGetCount(sa2);
    for (i = 0; i < n; i++) {
        str = sarrayGetString(sa2, i, L_NOCOPY);
        if (sarrayAddString(sa1, str, L_COPY) == 1) {
            L_ERROR("failed to add string at i = %d\n", __func__, i);
            return 1;
        }
    }
    return 0;
}


/*!
 * \brief   sarrayAppendRange()
 *
 * \param[in]    sa1     to be added to
 * \param[in]    sa2     append specified range of strings in sa2 to sa1
 * \param[in]    start   index of first string of sa2 to append
 * \param[in]    end     index of last string of sa2 to append;
 *                       -1 to append to end of array
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Copies of the strings in sarray2 are added to sarray1.
 *      (2) The [start ... end] range is truncated if necessary.
 *      (3) Use end == -1 to append to the end of sa2.
 * </pre>
 */
l_ok
sarrayAppendRange(SARRAY  *sa1,
                  SARRAY  *sa2,
                  l_int32  start,
                  l_int32  end)
{
char    *str;
l_int32  n, i;

    if (!sa1)
        return ERROR_INT("sa1 not defined", __func__, 1);
    if (!sa2)
        return ERROR_INT("sa2 not defined", __func__, 1);

    if (start < 0)
        start = 0;
    n = sarrayGetCount(sa2);
    if (end < 0 || end >= n)
        end = n - 1;
    if (start > end)
        return ERROR_INT("start > end", __func__, 1);

    for (i = start; i <= end; i++) {
        str = sarrayGetString(sa2, i, L_NOCOPY);
        sarrayAddString(sa1, str, L_COPY);
    }

    return 0;
}


/*----------------------------------------------------------------------*
 *          Pad an sarray to be the same size as another sarray         *
 *----------------------------------------------------------------------*/
/*!
 * \brief   sarrayPadToSameSize()
 *
 * \param[in]    sa1, sa2
 * \param[in]    padstring
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If two sarrays have different size, this adds enough
 *          instances of %padstring to the smaller so that they are
 *          the same size.  It is useful when two or more sarrays
 *          are being sequenced in parallel, and it is necessary to
 *          find a valid string at each index.
 * </pre>
 */
l_ok
sarrayPadToSameSize(SARRAY      *sa1,
                    SARRAY      *sa2,
                    const char  *padstring)
{
l_int32  i, n1, n2;

    if (!sa1 || !sa2)
        return ERROR_INT("both sa1 and sa2 not defined", __func__, 1);

    n1 = sarrayGetCount(sa1);
    n2 = sarrayGetCount(sa2);
    if (n1 < n2) {
        for (i = n1; i < n2; i++)
            sarrayAddString(sa1, padstring, L_COPY);
    } else if (n1 > n2) {
        for (i = n2; i < n1; i++)
            sarrayAddString(sa2, padstring, L_COPY);
    }

    return 0;
}


/*----------------------------------------------------------------------*
 *                   Convert word sarray to line sarray                 *
 *----------------------------------------------------------------------*/
/*!
 * \brief   sarrayConvertWordsToLines()
 *
 * \param[in]    sa  sa      of individual words
 * \param[in]    linesize    max num of chars in each line
 * \return  saout sa of formatted lines, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is useful for re-typesetting text to a specific maximum
 *          line length.  The individual words in the input sarray
 *          are concatenated into textlines.  An input word string of zero
 *          length is taken to be a paragraph separator.  Each time
 *          such a string is found, the current line is ended and
 *          a new line is also produced that contains just the
 *          string of zero length "".  When the output sarray
 *          of lines is eventually converted to a string with newlines
 *          typically appended to each line string, the empty
 *          strings are just converted to newlines, producing the visible
 *          paragraph separation.
 *      (2) What happens when a word is larger than linesize?
 *          We write it out as a single line anyway!  Words preceding
 *          or following this long word are placed on lines preceding
 *          or following the line with the long word.  Why this choice?
 *          Long "words" found in text documents are typically URLs, and
 *          it's often desirable not to put newlines in the middle of a URL.
 *          The text display program e.g., text editor will typically
 *          wrap the long "word" to fit in the window.
 * </pre>
 */
SARRAY *
sarrayConvertWordsToLines(SARRAY  *sa,
                          l_int32  linesize)
{
char    *wd, *strl;
char     emptystring[] = "";
l_int32  n, i, len, totlen;
SARRAY  *sal, *saout;

    if (!sa)
        return (SARRAY *)ERROR_PTR("sa not defined", __func__, NULL);

    saout = sarrayCreate(0);
    n = sarrayGetCount(sa);
    totlen = 0;
    sal = NULL;
    for (i = 0; i < n; i++) {
        if (!sal)
            sal = sarrayCreate(0);
        wd = sarrayGetString(sa, i, L_NOCOPY);
        len = strlen(wd);
        if (len == 0) {  /* end of paragraph: end line & insert blank line */
            if (totlen > 0) {
                strl = sarrayToString(sal, 2);
                sarrayAddString(saout, strl, L_INSERT);
            }
            sarrayAddString(saout, emptystring, L_COPY);
            sarrayDestroy(&sal);
            totlen = 0;
        } else if (totlen == 0 && len + 1 > linesize) {  /* long word! */
            sarrayAddString(saout, wd, L_COPY);  /* copy to one line */
        } else if (totlen + len + 1 > linesize) {  /* end line & start new */
            strl = sarrayToString(sal, 2);
            sarrayAddString(saout, strl, L_INSERT);
            sarrayDestroy(&sal);
            sal = sarrayCreate(0);
            sarrayAddString(sal, wd, L_COPY);
            totlen = len + 1;
        } else {  /* add to current line */
            sarrayAddString(sal, wd, L_COPY);
            totlen += len + 1;
        }
    }
    if (totlen > 0) {   /* didn't end with blank line; output last line */
        strl = sarrayToString(sal, 2);
        sarrayAddString(saout, strl, L_INSERT);
        sarrayDestroy(&sal);
    }

    return saout;
}


/*----------------------------------------------------------------------*
 *                    Split string on separator list                    *
 *----------------------------------------------------------------------*/
/*
 * \brief   sarraySplitString()
 *
 * \param[in]   sa            to append to; typically empty initially
 * \param[in]   str           string to split; not changed
 * \param[in]   separators    characters that split input string
 * \return   0 if OK, 1 on error.
 *
 * <pre>
 * Notes:
 *      (1) This uses strtokSafe().  See the notes there in utils.c.
 * </pre>
 */
l_int32
sarraySplitString(SARRAY      *sa,
                  const char  *str,
                  const char  *separators)
{
char  *cstr, *substr, *saveptr;

    if (!sa)
        return ERROR_INT("sa not defined", __func__, 1);
    if (!str)
        return ERROR_INT("str not defined", __func__, 1);
    if (!separators)
        return ERROR_INT("separators not defined", __func__, 1);

    cstr = stringNew(str);  /* preserves const-ness of input str */
    saveptr = NULL;
    substr = strtokSafe(cstr, separators, &saveptr);
    if (substr)
        sarrayAddString(sa, substr, L_INSERT);
    while ((substr = strtokSafe(NULL, separators, &saveptr)))
        sarrayAddString(sa, substr, L_INSERT);
    LEPT_FREE(cstr);

    return 0;
}


/*----------------------------------------------------------------------*
 *                              Filter sarray                           *
 *----------------------------------------------------------------------*/
/*!
 * \brief   sarraySelectBySubstring()
 *
 * \param[in]    sain     input sarray
 * \param[in]    substr   [optional] substring for matching; can be NULL
 * \return  saout output sarray, filtered with substring or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This selects all strings in sain that have substr as a substring.
 *          Note that we can't use strncmp() because we're looking for
 *          a match to the substring anywhere within each filename.
 *      (2) If substr == NULL, returns a copy of the sarray.
 * </pre>
 */
SARRAY *
sarraySelectBySubstring(SARRAY      *sain,
                        const char  *substr)
{
char    *str;
l_int32  n, i, offset, found;
SARRAY  *saout;

    if (!sain)
        return (SARRAY *)ERROR_PTR("sain not defined", __func__, NULL);

    n = sarrayGetCount(sain);
    if (!substr || n == 0)
        return sarrayCopy(sain);

    saout = sarrayCreate(n);
    for (i = 0; i < n; i++) {
        str = sarrayGetString(sain, i, L_NOCOPY);
        arrayFindSequence((l_uint8 *)str, strlen(str), (l_uint8 *)substr,
                          strlen(substr), &offset, &found);
        if (found)
            sarrayAddString(saout, str, L_COPY);
    }

    return saout;
}


/*!
 * \brief   sarraySelectRange()
 *
 * \param[in]    sain    input sarray
 * \param[in]    first   index of first string to be selected
 * \param[in]    last    index of last string to be selected;
 *                       use 0 to go to the end of the sarray
 * \return  saout   output sarray, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This makes %saout consisting of copies of all strings in %sain
 *          in the index set [first ... last].  Use %last == 0 to get all
 *          strings from %first to the last string in the sarray.
 * </pre>
 */
SARRAY *
sarraySelectRange(SARRAY  *sain,
                  l_int32  first,
                  l_int32  last)
{
char    *str;
l_int32  n, i;
SARRAY  *saout;

    if (!sain)
        return (SARRAY *)ERROR_PTR("sain not defined", __func__, NULL);
    if (first < 0) first = 0;
    n = sarrayGetCount(sain);
    if (last <= 0) last = n - 1;
    if (last >= n) {
        L_WARNING("last > n - 1; setting to n - 1\n", __func__);
        last = n - 1;
    }
    if (first > last)
        return (SARRAY *)ERROR_PTR("first must be >= last", __func__, NULL);

    saout = sarrayCreate(0);
    for (i = first; i <= last; i++) {
        str = sarrayGetString(sain, i, L_COPY);
        sarrayAddString(saout, str, L_INSERT);
    }

    return saout;
}


/*!
 * \brief   sarrayParseRange()
 *
 * \param[in]    sa             input sarray
 * \param[in]    start          index to start range search
 * \param[out]   pactualstart   index of actual start; may be > 'start'
 * \param[out]   pend           index of end
 * \param[out]   pnewstart      index of start of next range
 * \param[in]    substr         substring for matching at beginning of string
 * \param[in]    loc            byte offset within the string for the pattern;
 *                              use -1 if the location does not matter.
 * \return  0 if valid range found; 1 otherwise
 *
 * <pre>
 * Notes:
 *      (1) This finds the range of the next set of strings in SA,
 *          beginning the search at 'start', that does NOT have
 *          the substring 'substr' either at the indicated location
 *          in the string or anywhere in the string.  The input
 *          variable 'loc' is the specified offset within the string;
 *          use -1 to indicate 'anywhere in the string'.
 *      (2) Always check the return value to verify that a valid range
 *          was found.
 *      (3) If a valid range is not found, the values of actstart,
 *          end and newstart are all set to the size of sa.
 *      (4) If this is the last valid range, newstart returns the value n.
 *          In use, this should be tested before calling the function.
 *      (5) Usage example.  To find all the valid ranges in a file
 *          where the invalid lines begin with two dashes, copy each
 *          line in the file to a string in an sarray, and do:
 *             start = 0;
 *             while (!sarrayParseRange(sa, start, &actstart, &end, &start,
 *                    "--", 0))
 *                 lept_stderr("start = %d, end = %d\n", actstart, end);
 * </pre>
 */
l_int32
sarrayParseRange(SARRAY      *sa,
                 l_int32      start,
                 l_int32     *pactualstart,
                 l_int32     *pend,
                 l_int32     *pnewstart,
                 const char  *substr,
                 l_int32      loc)
{
char    *str;
l_int32  n, i, offset, found;

    if (!sa)
        return ERROR_INT("sa not defined", __func__, 1);
    if (!pactualstart || !pend || !pnewstart)
        return ERROR_INT("not all range addresses defined", __func__, 1);
    n = sarrayGetCount(sa);
    *pactualstart = *pend = *pnewstart = n;
    if (!substr)
        return ERROR_INT("substr not defined", __func__, 1);

        /* Look for the first string without the marker */
    if (start < 0 || start >= n)
        return 1;
    for (i = start; i < n; i++) {
        str = sarrayGetString(sa, i, L_NOCOPY);
        arrayFindSequence((l_uint8 *)str, strlen(str), (l_uint8 *)substr,
                          strlen(substr), &offset, &found);
        if (loc < 0) {
            if (!found) break;
        } else {
            if (!found || offset != loc) break;
        }
    }
    start = i;
    if (i == n)  /* couldn't get started */
        return 1;

        /* Look for the last string without the marker */
    *pactualstart = start;
    for (i = start + 1; i < n; i++) {
        str = sarrayGetString(sa, i, L_NOCOPY);
        arrayFindSequence((l_uint8 *)str, strlen(str), (l_uint8 *)substr,
                          strlen(substr), &offset, &found);
        if (loc < 0) {
            if (found) break;
        } else {
            if (found && offset == loc) break;
        }
    }
    *pend = i - 1;
    start = i;
    if (i == n)  /* no further range */
        return 0;

        /* Look for the first string after *pend without the marker.
         * This will start the next run of strings, if it exists. */
    for (i = start; i < n; i++) {
        str = sarrayGetString(sa, i, L_NOCOPY);
        arrayFindSequence((l_uint8 *)str, strlen(str), (l_uint8 *)substr,
                          strlen(substr), &offset, &found);
        if (loc < 0) {
            if (!found) break;
        } else {
            if (!found || offset != loc) break;
        }
    }
    if (i < n)
        *pnewstart = i;

    return 0;
}


/*----------------------------------------------------------------------*
 *                           Serialize for I/O                          *
 *----------------------------------------------------------------------*/
/*!
 * \brief   sarrayRead()
 *
 * \param[in]    filename
 * \return  sarray, or NULL on error
 */
SARRAY *
sarrayRead(const char  *filename)
{
FILE    *fp;
SARRAY  *sa;

    if (!filename)
        return (SARRAY *)ERROR_PTR("filename not defined", __func__, NULL);

    if ((fp = fopenReadStream(filename)) == NULL)
        return (SARRAY *)ERROR_PTR_1("stream not opened",
                                     filename, __func__, NULL);
    sa = sarrayReadStream(fp);
    fclose(fp);
    if (!sa)
        return (SARRAY *)ERROR_PTR_1("sa not read", filename, __func__, NULL);
    return sa;
}


/*!
 * \brief   sarrayReadStream()
 *
 * \param[in]    fp    file stream
 * \return  sarray, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) We store the size of each string along with the string.
 *          The limit on the number of strings is 50M.
 *          The limit on the size of any string is 2^30 bytes.
 *      (2) This allows a string to have embedded newlines.  By reading
 *          the entire string, as determined by its size, we are
 *          not affected by any number of embedded newlines.
 *      (3) It is OK for the sarray to be empty.
 * </pre>
 */
SARRAY *
sarrayReadStream(FILE  *fp)
{
char    *stringbuf;
l_int32  i, n, size, index, bufsize, version, ignore, success;
SARRAY  *sa;

    if (!fp)
        return (SARRAY *)ERROR_PTR("stream not defined", __func__, NULL);

    if (fscanf(fp, "\nSarray Version %d\n", &version) != 1)
        return (SARRAY *)ERROR_PTR("not an sarray file", __func__, NULL);
    if (version != SARRAY_VERSION_NUMBER)
        return (SARRAY *)ERROR_PTR("invalid sarray version", __func__, NULL);
    if (fscanf(fp, "Number of strings = %d\n", &n) != 1)
        return (SARRAY *)ERROR_PTR("error on # strings", __func__, NULL);
    if (n < 0)
        return (SARRAY *)ERROR_PTR("num string ptrs <= 0", __func__, NULL);
    if (n > (l_int32)MaxPtrArraySize)
        return (SARRAY *)ERROR_PTR("too many string ptrs", __func__, NULL);
    if (n == 0) L_INFO("the sarray is empty\n", __func__);

    success = TRUE;
    if ((sa = sarrayCreate(n)) == NULL)
        return (SARRAY *)ERROR_PTR("sa not made", __func__, NULL);
    bufsize = 512 + 1;
    stringbuf = (char *)LEPT_CALLOC(bufsize, sizeof(char));

    for (i = 0; i < n; i++) {
            /* Get the size of the stored string */
        if ((fscanf(fp, "%d[%d]:", &index, &size) != 2) || (size > (1 << 30))) {
            success = FALSE;
            L_ERROR("error on string size\n", __func__);
            goto cleanup;
        }
            /* Expand the string buffer if necessary */
        if (size > bufsize - 5) {
            LEPT_FREE(stringbuf);
            bufsize = (l_int32)(1.5 * size);
            stringbuf = (char *)LEPT_CALLOC(bufsize, sizeof(char));
        }
            /* Read the stored string, plus leading spaces and trailing \n */
        if (fread(stringbuf, 1, size + 3, fp) != size + 3) {
            success = FALSE;
            L_ERROR("error reading string\n", __func__);
            goto cleanup;
        }
            /* Remove the \n that was added by sarrayWriteStream() */
        stringbuf[size + 2] = '\0';
            /* Copy it in, skipping the 2 leading spaces */
        sarrayAddString(sa, stringbuf + 2, L_COPY);
    }
    ignore = fscanf(fp, "\n");

cleanup:
    LEPT_FREE(stringbuf);
    if (!success) sarrayDestroy(&sa);
    return sa;
}


/*!
 * \brief   sarrayReadMem()
 *
 * \param[in]    data    serialization in ascii
 * \param[in]    size    of data; can use strlen to get it
 * \return  sarray, or NULL on error
 */
SARRAY *
sarrayReadMem(const l_uint8  *data,
              size_t          size)
{
FILE    *fp;
SARRAY  *sa;

    if (!data)
        return (SARRAY *)ERROR_PTR("data not defined", __func__, NULL);
    if ((fp = fopenReadFromMemory(data, size)) == NULL)
        return (SARRAY *)ERROR_PTR("stream not opened", __func__, NULL);

    sa = sarrayReadStream(fp);
    fclose(fp);
    if (!sa) L_ERROR("sarray not read\n", __func__);
    return sa;
}


/*!
 * \brief   sarrayWrite()
 *
 * \param[in]    filename
 * \param[in]    sa          string array
 * \return  0 if OK; 1 on error
 */
l_ok
sarrayWrite(const char  *filename,
            SARRAY      *sa)
{
l_int32  ret;
FILE    *fp;

    if (!filename)
        return ERROR_INT("filename not defined", __func__, 1);
    if (!sa)
        return ERROR_INT("sa not defined", __func__, 1);

    if ((fp = fopenWriteStream(filename, "w")) == NULL)
        return ERROR_INT_1("stream not opened", filename, __func__, 1);
    ret = sarrayWriteStream(fp, sa);
    fclose(fp);
    if (ret)
        return ERROR_INT_1("sa not written to stream", filename, __func__, 1);
    return 0;
}


/*!
 * \brief   sarrayWriteStream()
 *
 * \param[in]    fp    file stream; use NULL to write to stderr
 * \param[in]    sa    string array
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This appends a '\n' to each string, which is stripped
 *          off by sarrayReadStream().
 * </pre>
 */
l_ok
sarrayWriteStream(FILE    *fp,
                  SARRAY  *sa)
{
l_int32  i, n, len;

    if (!fp)
        return ERROR_INT("stream not defined", __func__, 1);
    if (!sa)
        return sarrayWriteStderr(sa);

    n = sarrayGetCount(sa);
    fprintf(fp, "\nSarray Version %d\n", SARRAY_VERSION_NUMBER);
    fprintf(fp, "Number of strings = %d\n", n);
    for (i = 0; i < n; i++) {
        len = strlen(sa->array[i]);
        fprintf(fp, "  %d[%d]:  %s\n", i, len, sa->array[i]);
    }
    fprintf(fp, "\n");

    return 0;
}


/*!
 * \brief   sarrayWriteStderr()
 *
 * \param[in]    sa    string array
 * \return  0 if OK; 1 on error
 */
l_ok
sarrayWriteStderr(SARRAY  *sa)
{
l_int32  i, n, len;

    if (!sa)
        return ERROR_INT("sa not defined", __func__, 1);

    n = sarrayGetCount(sa);
    lept_stderr("\nSarray Version %d\n", SARRAY_VERSION_NUMBER);
    lept_stderr("Number of strings = %d\n", n);
    for (i = 0; i < n; i++) {
        len = strlen(sa->array[i]);
        lept_stderr("  %d[%d]:  %s\n", i, len, sa->array[i]);
    }
    lept_stderr("\n");
    return 0;
}


/*!
 * \brief   sarrayWriteMem()
 *
 * \param[out]   pdata    data of serialized sarray; ascii
 * \param[out]   psize    size of returned data
 * \param[in]    sa
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Serializes a sarray in memory and puts the result in a buffer.
 * </pre>
 */
l_ok
sarrayWriteMem(l_uint8  **pdata,
               size_t    *psize,
               SARRAY    *sa)
{
l_int32  ret;
FILE    *fp;

    if (pdata) *pdata = NULL;
    if (psize) *psize = 0;
    if (!pdata)
        return ERROR_INT("&data not defined", __func__, 1);
    if (!psize)
        return ERROR_INT("&size not defined", __func__, 1);
    if (!sa)
        return ERROR_INT("sa not defined", __func__, 1);

#if HAVE_FMEMOPEN
    if ((fp = open_memstream((char **)pdata, psize)) == NULL)
        return ERROR_INT("stream not opened", __func__, 1);
    ret = sarrayWriteStream(fp, sa);
    fputc('\0', fp);
    fclose(fp);
    if (*psize > 0) *psize = *psize - 1;
#else
    L_INFO("no fmemopen API --> work-around: write to temp file\n", __func__);
  #ifdef _WIN32
    if ((fp = fopenWriteWinTempfile()) == NULL)
        return ERROR_INT("tmpfile stream not opened", __func__, 1);
  #else
    if ((fp = tmpfile()) == NULL)
        return ERROR_INT("tmpfile stream not opened", __func__, 1);
  #endif  /* _WIN32 */
    ret = sarrayWriteStream(fp, sa);
    rewind(fp);
    *pdata = l_binaryReadStream(fp, psize);
    fclose(fp);
#endif  /* HAVE_FMEMOPEN */
    return ret;
}


/*!
 * \brief   sarrayAppend()
 *
 * \param[in]    filename
 * \param[in]    sa
 * \return  0 if OK; 1 on error
 */
l_ok
sarrayAppend(const char  *filename,
             SARRAY      *sa)
{
FILE  *fp;

    if (!filename)
        return ERROR_INT("filename not defined", __func__, 1);
    if (!sa)
        return ERROR_INT("sa not defined", __func__, 1);

    if ((fp = fopenWriteStream(filename, "a")) == NULL)
        return ERROR_INT_1("stream not opened", filename, __func__, 1);
    if (sarrayWriteStream(fp, sa)) {
        fclose(fp);
        return ERROR_INT_1("sa not appended to stream", filename, __func__, 1);
    }

    fclose(fp);
    return 0;
}


/*---------------------------------------------------------------------*
 *                           Directory filenames                       *
 *---------------------------------------------------------------------*/
/*!
 * \brief   getNumberedPathnamesInDirectory()
 *
 * \param[in]    dirname   directory name
 * \param[in]    substr    [optional] substring filter on filenames; can be NULL
 * \param[in]    numpre    number of characters in name before number
 * \param[in]    numpost   number of characters in name after the number,
 *                         up to a dot before an extension
 * \param[in]    maxnum    only consider page numbers up to this value
 * \return  sarray of numbered pathnames, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Returns the full pathnames of the numbered filenames in
 *          the directory.  The number in the filename is the index
 *          into the sarray.  For indices for which there are no filenames,
 *          an empty string ("") is placed into the sarray.
 *          This makes reading numbered files very simple.  For example,
 *          the image whose filename includes number N can be retrieved using
 *               pixReadIndexed(sa, N);
 *      (2) If %substr is not NULL, only filenames that contain
 *          the substring can be included.  If %substr is NULL,
 *          all matching filenames are used.
 *      (3) If no numbered files are found, it returns an empty sarray,
 *          with no initialized strings.
 *      (4) It is assumed that the page number is contained within
 *          the basename (the filename without directory or extension).
 *          %numpre is the number of characters in the basename
 *          preceding the actual page number; %numpost is the number
 *          following the page number, up to either the end of the
 *          basename or a ".", whichever comes first.
 *      (5) This is useful when all filenames contain numbers that are
 *          not necessarily consecutive.  0-padding is not required.
 *      (6) To use a O(n) matching algorithm, the largest page number
 *          is found and two internal arrays of this size are created.
 *          This maximum is constrained not to exceed %maxsum,
 *          to make sure that an unrealistically large number is not
 *          accidentally used to determine the array sizes.
 * </pre>
 */
SARRAY *
getNumberedPathnamesInDirectory(const char  *dirname,
                                const char  *substr,
                                l_int32      numpre,
                                l_int32      numpost,
                                l_int32      maxnum)
{
l_int32  nfiles;
SARRAY  *sa, *saout;

    if (!dirname)
        return (SARRAY *)ERROR_PTR("dirname not defined", __func__, NULL);

    if ((sa = getSortedPathnamesInDirectory(dirname, substr, 0, 0)) == NULL)
        return (SARRAY *)ERROR_PTR("sa not made", __func__, NULL);
    if ((nfiles = sarrayGetCount(sa)) == 0) {
        sarrayDestroy(&sa);
        return sarrayCreate(1);
    }

    saout = convertSortedToNumberedPathnames(sa, numpre, numpost, maxnum);
    sarrayDestroy(&sa);
    return saout;
}


/*!
 * \brief   getSortedPathnamesInDirectory()
 *
 * \param[in]    dirname   directory name
 * \param[in]    substr    [optional] substring filter on filenames; can be NULL
 * \param[in]    first     0-based
 * \param[in]    nfiles    use 0 for all to the end
 * \return  sarray of sorted pathnames, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Use %substr to filter filenames in the directory.  If
 *          %substr == NULL, this takes all files.
 *      (2) The files in the directory, after optional filtering by
 *          the substring, are lexically sorted in increasing order.
 *          Use %first and %nfiles to select a contiguous set of files.
 *      (3) The full pathnames are returned for the requested sequence.
 *          If no files are found after filtering, returns an empty sarray.
 * </pre>
 */
SARRAY *
getSortedPathnamesInDirectory(const char  *dirname,
                              const char  *substr,
                              l_int32      first,
                              l_int32      nfiles)
{
char    *fname, *fullname;
l_int32  i, n, last;
SARRAY  *sa, *safiles, *saout;

    if (!dirname)
        return (SARRAY *)ERROR_PTR("dirname not defined", __func__, NULL);

    if ((sa = getFilenamesInDirectory(dirname)) == NULL)
        return (SARRAY *)ERROR_PTR("sa not made", __func__, NULL);
    safiles = sarraySelectBySubstring(sa, substr);
    sarrayDestroy(&sa);
    n = sarrayGetCount(safiles);
    if (n == 0) {
        L_WARNING("no files found\n", __func__);
        return safiles;
    }

    sarraySort(safiles, safiles, L_SORT_INCREASING);

    first = L_MIN(L_MAX(first, 0), n - 1);
    if (nfiles == 0)
        nfiles = n - first;
    last = L_MIN(first + nfiles - 1, n - 1);

    saout = sarrayCreate(last - first + 1);
    for (i = first; i <= last; i++) {
        fname = sarrayGetString(safiles, i, L_NOCOPY);
        fullname = pathJoin(dirname, fname);
        sarrayAddString(saout, fullname, L_INSERT);
    }

    sarrayDestroy(&safiles);
    return saout;
}


/*!
 * \brief   convertSortedToNumberedPathnames()
 *
 * \param[in]    sa        sorted pathnames including zero-padded integers
 * \param[in]    numpre    number of characters in name before number
 * \param[in]    numpost   number of characters in name after the number,
 *                         up to a dot before an extension
 * \param[in]    maxnum    only consider page numbers up to this value
 * \return  sarray of numbered pathnames, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Typically, numpre = numpost = 0; e.g., when the filename
 *          just has a number followed by an optional extension.
 * </pre>
 */
SARRAY *
convertSortedToNumberedPathnames(SARRAY   *sa,
                                 l_int32   numpre,
                                 l_int32   numpost,
                                 l_int32   maxnum)
{
char    *fname, *str;
l_int32  i, nfiles, num, index;
SARRAY  *saout;

    if (!sa)
        return (SARRAY *)ERROR_PTR("sa not defined", __func__, NULL);
    if ((nfiles = sarrayGetCount(sa)) == 0)
        return sarrayCreate(1);

        /* Find the last file in the sorted array that has a number
         * that (a) matches the count pattern and (b) does not
         * exceed %maxnum.  %maxnum sets an upper limit on the size
         * of the sarray.  */
    num = 0;
    for (i = nfiles - 1; i >= 0; i--) {
        fname = sarrayGetString(sa, i, L_NOCOPY);
        num = extractNumberFromFilename(fname, numpre, numpost);
        if (num < 0) continue;
        num = L_MIN(num + 1, maxnum);
        break;
    }

    if (num <= 0)  /* none found */
        return sarrayCreate(1);

        /* Insert pathnames into the output sarray.
         * Ignore numbers that are out of the range of sarray. */
    saout = sarrayCreateInitialized(num, "");
    for (i = 0; i < nfiles; i++) {
        fname = sarrayGetString(sa, i, L_NOCOPY);
        index = extractNumberFromFilename(fname, numpre, numpost);
        if (index < 0 || index >= num) continue;
        str = sarrayGetString(saout, index, L_NOCOPY);
        if (str[0] != '\0') {
            L_WARNING("\n  Multiple files with same number: %d\n",
                      __func__, index);
        }
        sarrayReplaceString(saout, index, fname, L_COPY);
    }

    return saout;
}


/*!
 * \brief   getFilenamesInDirectory()
 *
 * \param[in]    dirname     directory name
 * \return  sarray of file names, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The versions compiled under unix and cygwin use the POSIX C
 *          library commands for handling directories.  For Windows,
 *          there is a separate implementation.
 *      (2) It returns an array of filename tails; i.e., only the part of
 *          the path after the last slash.
 *      (3) Use of the d_type field of dirent is not portable:
 *          "According to POSIX, the dirent structure contains a field
 *          char d_name[] of unspecified size, with at most NAME_MAX
 *          characters preceding the terminating null character.  Use
 *          of other fields will harm the portability of your programs."
 *      (4) As a consequence of (3), we note several things:
 *           ~ MINGW doesn't have a d_type member.
 *           ~ Older versions of gcc (e.g., 2.95.3) return DT_UNKNOWN
 *             for d_type from all files.
 *          On these systems, this function will return directories
 *          (except for '.' and '..', which are eliminated using
 *          the d_name field).
 *      (5) For unix, we avoid the bug in earlier versions of realpath()
 *          by requiring either POSIX 2008 or use of glibc.
 *          
 * </pre>
 */

#ifndef _WIN32

SARRAY *
getFilenamesInDirectory(const char  *dirname)
{
char           *gendir, *realdir, *stat_path;
size_t          size;
SARRAY         *safiles;
DIR            *pdir;
struct dirent  *pdirentry;
int             dfd, stat_ret;
struct stat     st;

    if (!dirname)
        return (SARRAY *)ERROR_PTR("dirname not defined", __func__, NULL);
    if (dirname[0] == '\0')
        return (SARRAY *)ERROR_PTR("dirname is empty", __func__, NULL);

        /* Who would have thought it was this fiddly to open a directory
           and get the files inside?  fstatat() works with relative
           directory paths, and stat() requires using the absolute path.
           realpath() works as follows for files and directories:
            * If the file or directory exists, realpath returns its path;
              else it returns NULL.
            * For realpath() we use the POSIX 2008 implementation, where
              the second arg is NULL and the path is malloc'd and returned
              if the file or directory exists.  All versions of glibc
              support this.  */
    gendir = genPathname(dirname, NULL);
    realdir = realpath(gendir, NULL);
    LEPT_FREE(gendir);
    if (realdir == NULL)
        return (SARRAY *)ERROR_PTR("realdir not made", __func__, NULL);
    if ((pdir = opendir(realdir)) == NULL) {
        L_ERROR("directory %s not opened\n", __func__, realdir);
        LEPT_FREE(realdir);
        return NULL;
    }
    safiles = sarrayCreate(0);
    while ((pdirentry = readdir(pdir))) {
#if HAVE_DIRFD && HAVE_FSTATAT
            /* Platform issues: although Linux has these POSIX functions,
             * AIX doesn't have fstatat() and Solaris doesn't have dirfd(). */
        dfd = dirfd(pdir);
        stat_ret = fstatat(dfd, pdirentry->d_name, &st, 0);
#else
        size = strlen(realdir) + strlen(pdirentry->d_name) + 2;
        stat_path = (char *)LEPT_CALLOC(size, 1);
        snprintf(stat_path, size, "%s/%s", realdir, pdirentry->d_name);
        stat_ret = stat(stat_path, &st);
        LEPT_FREE(stat_path);
#endif
        if (stat_ret == 0 && S_ISDIR(st.st_mode))
            continue;
        sarrayAddString(safiles, pdirentry->d_name, L_COPY);
    }
    closedir(pdir);
    LEPT_FREE(realdir);
    return safiles;
}

#else  /* _WIN32 */

    /* http://msdn2.microsoft.com/en-us/library/aa365200(VS.85).aspx */
#include <windows.h>

SARRAY *
getFilenamesInDirectory(const char  *dirname)
{
char             *pszDir;
char             *realdir;
HANDLE            hFind = INVALID_HANDLE_VALUE;
SARRAY           *safiles;
WIN32_FIND_DATAA  ffd;

    if (!dirname)
        return (SARRAY *)ERROR_PTR("dirname not defined", __func__, NULL);

    realdir = genPathname(dirname, NULL);
    pszDir = stringJoin(realdir, "\\*");
    LEPT_FREE(realdir);

    if (strlen(pszDir) + 1 > MAX_PATH) {
        LEPT_FREE(pszDir);
        return (SARRAY *)ERROR_PTR("dirname is too long", __func__, NULL);
    }

    if ((safiles = sarrayCreate(0)) == NULL) {
        LEPT_FREE(pszDir);
        return (SARRAY *)ERROR_PTR("safiles not made", __func__, NULL);
    }

    hFind = FindFirstFileA(pszDir, &ffd);
    if (INVALID_HANDLE_VALUE == hFind) {
        sarrayDestroy(&safiles);
        LEPT_FREE(pszDir);
        return (SARRAY *)ERROR_PTR("hFind not opened", __func__, NULL);
    }

    while (FindNextFileA(hFind, &ffd) != 0) {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)  /* skip dirs */
            continue;
        convertSepCharsInPath(ffd.cFileName, UNIX_PATH_SEPCHAR);
        sarrayAddString(safiles, ffd.cFileName, L_COPY);
    }

    FindClose(hFind);
    LEPT_FREE(pszDir);
    return safiles;
}
#endif  /* _WIN32 */
