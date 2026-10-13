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
 * \file utils2.c
 * <pre>
 *
 *      ------------------------------------------
 *      This file has these utilities:
 *         - safe string operations
 *         - find/replace operations on strings
 *         - read/write between file and memory
 *         - multi-platform file and directory operations
 *         - file name operations
 *      ------------------------------------------
 *
 *       Safe string procs
 *           char      *stringNew()
 *           l_int32    stringCopy()
 *           l_int32    stringCopySegment()
 *           l_int32    stringReplace()
 *           l_int32    stringLength()
 *           l_int32    stringCat()
 *           char      *stringConcatNew()
 *           char      *stringJoin()
 *           l_int32    stringJoinIP()
 *           char      *stringReverse()
 *           char      *strtokSafe()
 *           l_int32    stringSplitOnToken()
 *
 *       Find and replace string and array procs
 *           l_int32    stringCheckForChars()
 *           char      *stringRemoveChars()
 *           char      *stringReplaceEachSubstr()
 *           char      *stringReplaceSubstr()
 *           L_DNA     *stringFindEachSubstr()
 *           l_int32    stringFindSubstr()
 *           l_uint8   *arrayReplaceEachSequence()
 *           L_DNA     *arrayFindEachSequence()
 *           l_int32    arrayFindSequence()
 *
 *       Safe realloc
 *           void      *reallocNew()
 *
 *       Read and write between file and memory
 *           l_uint8   *l_binaryRead()
 *           l_uint8   *l_binaryReadStream()
 *           l_uint8   *l_binaryReadSelect()
 *           l_uint8   *l_binaryReadSelectStream()
 *           l_int32    l_binaryWrite()
 *           l_int32    nbytesInFile()
 *           l_int32    fnbytesInFile()
 *
 *       Copy and compare in memory
 *           l_uint8   *l_binaryCopy()
 *           l_uint8   *l_binaryCompare()
 *
 *       File copy operations
 *           l_int32    fileCopy()
 *           l_int32    fileConcatenate()
 *           l_int32    fileAppendString()
 *
 *       File split operations
 *           l_int32    fileSplitLinesUniform()
 *
 *       Multi-platform functions for opening file streams
 *           FILE      *fopenReadStream()
 *           FILE      *fopenWriteStream()
 *           FILE      *fopenReadFromMemory()
 *
 *       Opening a Windows tmpfile for writing
 *           FILE      *fopenWriteWinTempfile()
 *
 *       Multi-platform functions that avoid C-runtime boundary crossing
 *       with Windows DLLs  (use in programs only)
 *           FILE      *lept_fopen()
 *           l_int32    lept_fclose()
 *           void      *lept_calloc()
 *           void       lept_free()
 *
 *       Multi-platform file system operations in temp directories
 *           l_int32    lept_mkdir()
 *           l_int32    lept_rmdir()
 *           l_int32    lept_direxists()
 *           l_int32    lept_mv()
 *           l_int32    lept_rm_match()
 *           l_int32    lept_rm()
 *           l_int32    lept_rmfile()
 *           l_int32    lept_cp()
 *
 *       Special debug/test function for calling 'system'
 *           l_int32    callSystemDebug()
 *
 *       General file name operations
 *           l_int32    splitPathAtDirectory()
 *           l_int32    splitPathAtExtension()
 *           char      *pathJoin()
 *           char      *appendSubdirs()
 *
 *       Special file name operations
 *           l_int32    convertSepCharsInPath()
 *           char      *genPathname()
 *           l_int32    makeTempDirname()
 *           l_int32    modifyTrailingSlash()
 *           char      *l_makeTempFilename()
 *           l_int32    extractNumberFromFilename()
 *
 *
 *  Notes on multi-platform development
 *  -----------------------------------
 *  This is important:
 *  (1) With the exception of splitPathAtDirectory(), splitPathAtExtension()
  *     and genPathname(), all input pathnames must have unix separators.
 *  (2) On macOS, iOS and Windows, for read or write to "/tmp/..."
 *      the filename is rewritten to use the OS specific temp directory:
 *         /tmp  ==>   [Temp]/...
 *  (3) This filename rewrite, along with the conversion from unix
 *      to OS specific pathnames, happens in genPathname().
 *  (4) Use fopenReadStream() and fopenWriteStream() to open files,
 *      because these use genPathname() to find the platform-dependent
 *      filenames.  Likewise for l_binaryRead() and l_binaryWrite().
 *  (5) For moving, copying and removing files and directories that are in
 *      subdirectories of /tmp, use the lept_*() file system shell wrappers:
 *         lept_mkdir(), lept_rmdir(), lept_mv(), lept_rm() and lept_cp().
 *  (6) For programs use the lept_fopen(), lept_fclose(), lept_calloc()
 *      and lept_free() C library wrappers.  These work properly on Windows,
 *      where the same DLL must perform complementary operations on
 *      file streams (open/close) and heap memory (malloc/free).
 *  (7) Why read and write files to temp directories?
 *      The library needs the ability to read and write ephemeral
 *      files to default places, both for generating debugging output
 *      and for supporting regression tests.  Applications also need
 *      this ability for debugging.
 *  (8) Why do the pathname rewrite on macOS, iOS and Windows?
 *      The goal is to have the library, and programs using the library,
 *      run on multiple platforms without changes.  The location of
 *      temporary files depends on the platform as well as the user's
 *      configuration.  Temp files on some operating systems are in some
 *      directory not known a priori.  To make everything work seamlessly on
 *      any OS, every time you open a file for reading or writing,
 *      use a special function such as fopenReadStream() or
 *      fopenWriteStream(); these call genPathname() to ensure that
 *      if it is a temp file, the correct path is used.  To indicate
 *      that this is a temp file, the application is written with the
 *      root directory of the path in a canonical form: "/tmp".
 *  (9) Why is it that multi-platform directory functions like lept_mkdir()
 *      and lept_rmdir(), as well as associated file functions like
 *      lept_rm(), lept_mv() and lept_cp(), only work in the temp dir?
 *      These functions were designed to provide easy manipulation of
 *      temp files.  The restriction to temp files is for safety -- to
 *      prevent an accidental deletion of important files.  For example,
 *      lept_rmdir() first deletes all files in a specified subdirectory
 *      of temp, and then removes the directory.
 *
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#ifdef _MSC_VER
#include <process.h>
#include <direct.h>
#define getcwd _getcwd  /* fix MSVC warning */
#else
#include <unistd.h>
#endif   /* _MSC_VER */

#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>     /* _O_CREAT, ... */
#include <io.h>        /* _open */
#include <sys/stat.h>  /* _S_IREAD, _S_IWRITE */
#else
#include <sys/stat.h>  /* for stat, mkdir(2) */
#include <sys/types.h>
#endif

#ifdef __APPLE__
#include <unistd.h>
#include <errno.h>
#endif

#include <string.h>
#include <stddef.h>
#include "allheaders.h"

#if defined(__APPLE__) || defined(_WIN32)
/* Rewrite paths starting with /tmp for macOS, iOS and Windows. */
#define REWRITE_TMP
#endif

/*--------------------------------------------------------------------*
 *                       Safe string operations                       *
 *--------------------------------------------------------------------*/
/*!
 * \brief   stringNew()
 *
 * \param[in]    src
 * \return  dest copy of %src string, or NULL on error
 */
char *
stringNew(const char  *src)
{
l_int32  len;
char    *dest;

    if (!src) {
        L_WARNING("src not defined\n", __func__);
        return NULL;
    }

    len = strlen(src);
    if ((dest = (char *)LEPT_CALLOC(len + 1, sizeof(char))) == NULL)
        return (char *)ERROR_PTR("dest not made", __func__, NULL);

    stringCopy(dest, src, len);
    return dest;
}


/*!
 * \brief   stringCopy()
 *
 * \param[in]    dest    existing byte buffer
 * \param[in]    src     string [optional] can be null
 * \param[in]    n       max number of characters to copy
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Relatively safe wrapper for strncpy, that checks the input,
 *          and does not complain if %src is null or %n < 1.
 *          If %n < 1, this is a no-op.
 *      (2) %dest needs to be at least %n bytes in size.
 *      (3) We don't call strncpy() because valgrind complains about
 *          use of uninitialized values.
 * </pre>
 */
l_ok
stringCopy(char        *dest,
           const char  *src,
           l_int32      n)
{
l_int32  i;

    if (!dest)
        return ERROR_INT("dest not defined", __func__, 1);
    if (!src || n < 1)
        return 0;

        /* Implementation of strncpy that valgrind doesn't complain about */
    for (i = 0; i < n && src[i] != '\0'; i++)
        dest[i] = src[i];
    for (; i < n; i++)
        dest[i] = '\0';
    return 0;
}


/*!
 * \brief   stringCopySegment()
 *
 *
 * \param[in]    src      string
 * \param[in]    start    byte position at start of segment
 * \param[in]    nbytes   number of bytes in the segment; use 0 to go to end
 * \return  copy of segment, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is a variant of stringNew() that makes a new string
 *          from a segment of the input string.  The segment is specified
 *          by the starting position and the number of bytes.
 *      (2) The start location %start must be within the string %src.
 *      (3) The copy is truncated to the end of the source string.
 *          Use %nbytes = 0 to copy to the end of %src.
 * </pre>
 */
char *
stringCopySegment(const char  *src,
                  l_int32      start,
                  l_int32      nbytes)
{
char    *dest;
l_int32  len;

    if (!src)
        return (char *)ERROR_PTR("src not defined", __func__, NULL);
    len = strlen(src);
    if (start < 0 || start > len - 1)
        return (char *)ERROR_PTR("invalid start", __func__, NULL);
    if (nbytes <= 0)  /* copy to the end */
        nbytes = len - start;
    if (start + nbytes > len)  /* truncate to the end */
        nbytes = len - start;
    if ((dest = (char *)LEPT_CALLOC(nbytes + 1, sizeof(char))) == NULL)
        return (char *)ERROR_PTR("dest not made", __func__, NULL);
    stringCopy(dest, src + start, nbytes);
    return dest;
}


/*!
 * \brief   stringReplace()
 *
 * \param[out]   pdest    string copy
 * \param[in]    src      [optional] string; can be null
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Frees any existing dest string
 *      (2) Puts a copy of src string in the dest
 *      (3) If either or both strings are null, does something reasonable.
 * </pre>
 */
l_ok
stringReplace(char       **pdest,
              const char  *src)
{
    if (!pdest)
        return ERROR_INT("pdest not defined", __func__, 1);

    if (*pdest)
        LEPT_FREE(*pdest);

    if (src)
        *pdest = stringNew(src);
    else
        *pdest = NULL;
    return 0;
}


/*!
 * \brief   stringLength()
 *
 * \param[in]    src    string can be null or NULL-terminated string
 * \param[in]    size   number of bytes to check; e.g., size of src buffer
 * \return  length of src in bytes; 0 if no bytes are found;
 *                                  %size on error when NUL byte is not found.
 *
 * <pre>
 * Notes:
 *      (1) Safe implementation of strlen that only checks %size bytes
 *          for trailing NUL.
 *      (2) Valid returned string lengths are between 0 and size - 1.
 *          If %size bytes are checked without finding a NUL byte, then
 *          an error is indicated by returning %size.
 * </pre>
 */
l_int32
stringLength(const char  *src,
             size_t       size)
{
l_int32  i;

    if (!src)
        return 0;
    if (size < 1)
        return ERROR_INT("size < 1; too small", __func__, 0);

    for (i = 0; i < size; i++) {
        if (src[i] == '\0')
            return i;
    }

        /* Didn't find a NUL byte */
    L_ERROR("NUL byte not found in %zu bytes\n", __func__, size);
    return size;
}


/*!
 * \brief   stringCat()
 *
 * \param[in]    dest    null-terminated byte buffer
 * \param[in]    size    size of dest buffer
 * \param[in]    src     string can be null or NULL-terminated string
 * \return  number of bytes added to dest; -1 on error
 *
 * <pre>
 * Notes:
 *      (1) Alternative implementation of strncat, that checks the input,
 *          is easier to use (since the size of the dest buffer is specified
 *          rather than the number of bytes to copy), and does not complain
 *          if %src is null.
 *      (2) Never writes past end of dest.
 *      (3) If there is not enough room to append the src, which is an error,
 *          it does nothing.
 *      (4) N.B. The order of 2nd and 3rd args is reversed from that in
 *          strncat, as in the Windows function strcat_s().
 * </pre>
 */
l_int32
stringCat(char        *dest,
          size_t       size,
          const char  *src)
{
l_int32  i, n;
l_int32  lendest, lensrc;

    if (!dest)
        return ERROR_INT("dest not defined", __func__, -1);
    if (size < 1)
        return ERROR_INT("size < 1; too small", __func__, -1);
    if (!src)
        return 0;

    lendest = stringLength(dest, size);
    if (lendest == size)
        return ERROR_INT("no terminating nul byte", __func__, -1);
    lensrc = stringLength(src, size);
    if (lensrc == 0)
        return 0;  /* nothing added to dest */
    n = (lendest + lensrc > size - 1) ? 0 : lensrc;
    if (n == 0)
        return ERROR_INT("dest too small for append", __func__, -1);

    for (i = 0; i < n; i++)
        dest[lendest + i] = src[i];
    dest[lendest + n] = '\0';
    return n;
}


/*!
 * \brief   stringConcatNew()
 *
 * \param[in]    first    first string in list
 * \param[in]    ...      NULL-terminated list of strings
 * \return  result new string concatenating the input strings, or
 *                      NULL if first == NULL
 *
 * <pre>
 * Notes:
 *      (1) The last arg in the list of strings must be NULL.
 *      (2) Caller must free the returned string.
 * </pre>
 */
char *
stringConcatNew(const char  *first, ...)
{
size_t       len;
char        *result, *ptr;
const char  *arg;
va_list      args;

    if (!first) return NULL;

        /* Find the length of the output string */
    va_start(args, first);
    len = strlen(first);
    while ((arg = va_arg(args, const char *)) != NULL)
        len += strlen(arg);
    va_end(args);
    result = (char *)LEPT_CALLOC(len + 1, sizeof(char));

        /* Concatenate the args */
    va_start(args, first);
    ptr = result;
    arg = first;
    while (*arg)
        *ptr++ = *arg++;
    while ((arg = va_arg(args, const char *)) != NULL) {
        while (*arg)
            *ptr++ = *arg++;
    }
    va_end(args);
    return result;
}


/*!
 * \brief   stringJoin()
 *
 * \param[in]    src1    [optional] string; can be null
 * \param[in]    src2    [optional] string; can be null
 * \return  concatenated string, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is a safe version of strcat; it makes a new string.
 *      (2) It is not an error if either or both of the strings
 *          are empty, or if either or both of the pointers are null.
 * </pre>
 */
char *
stringJoin(const char  *src1,
           const char  *src2)
{
char    *dest;
l_int32  srclen1, srclen2, destlen;

    srclen1 = (src1) ? strlen(src1) : 0;
    srclen2 = (src2) ? strlen(src2) : 0;
    destlen = srclen1 + srclen2 + 3;

    if ((dest = (char *)LEPT_CALLOC(destlen, sizeof(char))) == NULL)
        return (char *)ERROR_PTR("calloc fail for dest", __func__, NULL);

    if (src1)
        stringCat(dest, destlen, src1);
    if (src2)
        stringCat(dest, destlen, src2);
    return dest;
}


/*!
 * \brief   stringJoinIP()
 *
 * \param[in,out]  psrc1   address of string src1; cannot be on the stack
 * \param[in]      src2    [optional] string; can be null
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is a safe in-place version of strcat.  The contents of
 *          src1 is replaced by the concatenation of src1 and src2.
 *      (2) It is not an error if either or both of the strings
 *          are empty (""), or if the pointers to the strings (*psrc1, src2)
 *          are null.
 *      (3) src1 should be initialized to null or an empty string
 *          before the first call.  Use one of these:
 *              char *src1 = NULL;
 *              char *src1 = stringNew("");
 *          Then call with:
 *              stringJoinIP(&src1, src2);
 *      (4) This can also be implemented as a macro:
 * \code
 *              #define stringJoinIP(src1, src2) \
 *                  {tmpstr = stringJoin((src1),(src2)); \
 *                  LEPT_FREE(src1); \
 *                  (src1) = tmpstr;}
 * \endcode
 *      (5) Another function to consider for joining many strings is
 *          stringConcatNew().
 * </pre>
 */
l_ok
stringJoinIP(char       **psrc1,
             const char  *src2)
{
char  *tmpstr;

    if (!psrc1)
        return ERROR_INT("&src1 not defined", __func__, 1);

    tmpstr = stringJoin(*psrc1, src2);
    LEPT_FREE(*psrc1);
    *psrc1 = tmpstr;
    return 0;
}


/*!
 * \brief   stringReverse()
 *
 * \param[in]    src    string
 * \return  dest newly-allocated reversed string
 */
char *
stringReverse(const char  *src)
{
char    *dest;
l_int32  i, len;

    if (!src)
        return (char *)ERROR_PTR("src not defined", __func__, NULL);
    len = strlen(src);
    if ((dest = (char *)LEPT_CALLOC(len + 1, sizeof(char))) == NULL)
        return (char *)ERROR_PTR("calloc fail for dest", __func__, NULL);
    for (i = 0; i < len; i++)
        dest[i] = src[len - 1 - i];

    return dest;
}


/*!
 * \brief   strtokSafe()
 *
 * \param[in]    cstr      input string to be sequentially parsed;
 *                         use NULL after the first call
 * \param[in]    seps      a string of character separators
 * \param[out]   psaveptr  ptr to the next char after
 *                         the last encountered separator
 * \return  substr         a new string that is copied from the previous
 *                         saveptr up to but not including the next
 *                         separator character, or NULL if end of cstr.
 *
 * <pre>
 * Notes:
 *      (1) This is a thread-safe implementation of strtok.
 *      (2) It has the same interface as strtok_r.
 *      (3) It differs from strtok_r in usage in two respects:
 *          (a) the input string is not altered
 *          (b) each returned substring is newly allocated and must
 *              be freed after use.
 *      (4) Let me repeat that.  This is "safe" because the input
 *          string is not altered and because each returned string
 *          is newly allocated on the heap.
 *      (5) It is here because, surprisingly, some C libraries don't
 *          include strtok_r.
 *      (6) Important usage points:
 *          ~ Input the string to be parsed on the first invocation.
 *          ~ Then input NULL after that; the value returned in saveptr
 *            is used in all subsequent calls.
 *      (7) This is only slightly slower than strtok_r.
 * </pre>
 */
char *
strtokSafe(char        *cstr,
           const char  *seps,
           char       **psaveptr)
{
char     nextc;
char    *start, *substr;
l_int32  istart, i, j, nchars;

    if (!seps)
        return (char *)ERROR_PTR("seps not defined", __func__, NULL);
    if (!psaveptr)
        return (char *)ERROR_PTR("&saveptr not defined", __func__, NULL);

    if (!cstr) {
        start = *psaveptr;
    } else {
        start = cstr;
        *psaveptr = NULL;
    }
    if (!start)  /* nothing to do */
        return NULL;

        /* First time, scan for the first non-sep character */
    istart = 0;
    if (cstr) {
        for (istart = 0;; istart++) {
            if ((nextc = start[istart]) == '\0') {
                *psaveptr = NULL;  /* in case caller doesn't check ret value */
                return NULL;
            }
            if (!strchr(seps, nextc))
                break;
        }
    }

        /* Scan through, looking for a sep character; if none is
         * found, 'i' will be at the end of the string. */
    for (i = istart;; i++) {
        if ((nextc = start[i]) == '\0')
            break;
        if (strchr(seps, nextc))
            break;
    }

        /* Save the substring */
    nchars = i - istart;
    substr = (char *)LEPT_CALLOC(nchars + 1, sizeof(char));
    stringCopy(substr, start + istart, nchars);

        /* Look for the next non-sep character.
         * If this is the last substring, return a null saveptr. */
    for (j = i;; j++) {
        if ((nextc = start[j]) == '\0') {
            *psaveptr = NULL;  /* no more non-sep characters */
            break;
        }
        if (!strchr(seps, nextc)) {
            *psaveptr = start + j;  /* start here on next call */
                break;
        }
    }

    return substr;
}


/*!
 * \brief   stringSplitOnToken()
 *
 * \param[in]    cstr     input string to be split; not altered
 * \param[in]    seps     a string of character separators
 * \param[out]   phead    ptr to copy of the input string, up to
 *                        the first separator token encountered
 * \param[out]   ptail    ptr to copy of the part of the input string
 *                        starting with the first non-separator character
 *                        that occurs after the first separator is found
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The input string is not altered; all split parts are new strings.
 *      (2) The split occurs around the first consecutive sequence of
 *          tokens encountered.
 *      (3) The head goes from the beginning of the string up to
 *          but not including the first token found.
 *      (4) The tail contains the second part of the string, starting
 *          with the first char in that part that is NOT a token.
 *      (5) If no separator token is found, 'head' contains a copy
 *          of the input string and 'tail' is null.
 * </pre>
 */
l_ok
stringSplitOnToken(char        *cstr,
                   const char  *seps,
                   char       **phead,
                   char       **ptail)
{
char  *saveptr;

    if (!phead)
        return ERROR_INT("&head not defined", __func__, 1);
    if (!ptail)
        return ERROR_INT("&tail not defined", __func__, 1);
    *phead = *ptail = NULL;
    if (!cstr)
        return ERROR_INT("cstr not defined", __func__, 1);
    if (!seps)
        return ERROR_INT("seps not defined", __func__, 1);

    *phead = strtokSafe(cstr, seps, &saveptr);
    if (saveptr)
        *ptail = stringNew(saveptr);
    return 0;
}


/*--------------------------------------------------------------------*
 *                       Find and replace procs                       *
 *--------------------------------------------------------------------*/
/*!
 * \brief   stringCheckForChars()
 *
 * \param[in]    src      input string; can be of zero length
 * \param[in]    chars    string of chars to be searched for in %src
 * \param[out]   pfound   1 if any characters are found; 0 otherwise
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This can be used to sanitize an operation by checking for
 *          special characters that don't belong in a string.
 * </pre>
 */
l_ok
stringCheckForChars(const char  *src,
                    const char  *chars,
                    l_int32     *pfound)
{
char     ch;
l_int32  i, n;

    if (!pfound)
        return ERROR_INT("&found not defined", __func__, 1);
    *pfound = FALSE;
    if (!src || !chars)
        return ERROR_INT("src and chars not both defined", __func__, 1);

    n = strlen(src);
    for (i = 0; i < n; i++) {
        ch = src[i];
        if (strchr(chars, ch)) {
            *pfound = TRUE;
            break;
        }
    }
    return 0;
}


/*!
 * \brief   stringRemoveChars()
 *
 * \param[in]    src        input string; can be of zero length
 * \param[in]    remchars   string of chars to be removed from src
 * \return  dest string with specified chars removed, or NULL on error
 */
char *
stringRemoveChars(const char  *src,
                  const char  *remchars)
{
char     ch;
char    *dest;
l_int32  nsrc, i, k;

    if (!src)
        return (char *)ERROR_PTR("src not defined", __func__, NULL);
    if (!remchars)
        return stringNew(src);

    if ((dest = (char *)LEPT_CALLOC(strlen(src) + 1, sizeof(char))) == NULL)
        return (char *)ERROR_PTR("dest not made", __func__, NULL);
    nsrc = strlen(src);
    for (i = 0, k = 0; i < nsrc; i++) {
        ch = src[i];
        if (!strchr(remchars, ch))
            dest[k++] = ch;
    }

    return dest;
}


/*!
 * \brief   stringReplaceEachSubstr()
 *
 * \param[in]    src      input string; can be of zero length
 * \param[in]    sub1     substring to be replaced
 * \param[in]    sub2     substring to put in; can be ""
 * \param[out]   pcount   [optional] the number of times that sub1
 *                        is found in src; 0 if not found
 * \return  dest string with substring replaced, or NULL if the
 *              substring not found or on error.
 *
 * <pre>
 * Notes:
 *      (1) This is a wrapper for simple string substitution that uses
 *          the more general function arrayReplaceEachSequence().
 *      (2) This finds every non-overlapping occurrence of %sub1 in
 *          %src, and replaces it with %sub2.  By "non-overlapping"
 *          we mean that after it finds each match, it removes the
 *          matching characters, replaces with the substitution string
 *          (if not empty), and continues.  For example, if you replace
 *          'aa' by 'X' in 'baaabbb', you find one match at position 1
 *          and return 'bXabbb'.
 *      (3) To only remove each instance of sub1, use "" for sub2
 *      (4) Returns a copy of %src if sub1 and sub2 are the same.
 *      (5) If the input %src is binary data that can have null characters,
 *          use arrayReplaceEachSequence() directly.
 * </pre>
 */
char *
stringReplaceEachSubstr(const char  *src,
                        const char  *sub1,
                        const char  *sub2,
                        l_int32     *pcount)
{
size_t  datalen;

    if (pcount) *pcount = 0;
    if (!src || !sub1 || !sub2)
        return (char *)ERROR_PTR("src, sub1, sub2 not all defined",
                                 __func__, NULL);

    if (strlen(sub2) > 0) {
        return (char *)arrayReplaceEachSequence(
                               (const l_uint8 *)src, strlen(src),
                               (const l_uint8 *)sub1, strlen(sub1),
                               (const l_uint8 *)sub2, strlen(sub2),
                               &datalen, pcount);
    } else {  /* empty replacement string; removal only */
        return (char *)arrayReplaceEachSequence(
                               (const l_uint8 *)src, strlen(src),
                               (const l_uint8 *)sub1, strlen(sub1),
                               NULL, 0, &datalen, pcount);
    }
}


/*!
 * \brief   stringReplaceSubstr()
 *
 * \param[in]      src      input string; can be of zero length
 * \param[in]      sub1     substring to be replaced
 * \param[in]      sub2     substring to put in; can be ""
 * \param[in,out]  ploc     [optional] input start location for search;
 *                          returns the loc after replacement
 * \param[out]     pfound   [optional] 1 if sub1 is found; 0 otherwise
 * \return  dest string with substring replaced, or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) Replaces the first instance.
 *      (2) To remove sub1 without replacement, use "" for sub2.
 *      (3) Returns a copy of %src if either no instance of %sub1 is found,
 *          or if %sub1 and %sub2 are the same.
 *      (4) If %ploc == NULL, the search will start at the beginning of %src.
 *          If %ploc != NULL, *ploc must be initialized to the byte offset
 *          within %src from which the search starts.  To search the
 *          string from the beginning, set %loc = 0 and input &loc.
 *          After finding %sub1 and replacing it with %sub2, %loc will be
 *          returned as the next position after %sub2 in the output string.
 *      (5) Note that the output string also includes all the characters
 *          from the input string that occur after the single substitution.
 * </pre>
 */
char *
stringReplaceSubstr(const char  *src,
                    const char  *sub1,
                    const char  *sub2,
                    l_int32     *ploc,
                    l_int32     *pfound)
{
const char  *ptr;
char        *dest;
l_int32      nsrc, nsub1, nsub2, len, npre, loc;

    if (pfound) *pfound = 0;
    if (!src || !sub1 || !sub2)
        return (char *)ERROR_PTR("src, sub1, sub2 not all defined",
                                 __func__, NULL);

    if (ploc)
        loc = *ploc;
    else
        loc = 0;
    if (!strcmp(sub1, sub2))
        return stringNew(src);
    if ((ptr = strstr(src + loc, sub1)) == NULL)
        return stringNew(src);
    if (pfound) *pfound = 1;

    nsrc = strlen(src);
    nsub1 = strlen(sub1);
    nsub2 = strlen(sub2);
    len = nsrc + nsub2 - nsub1;
    if ((dest = (char *)LEPT_CALLOC(len + 1, sizeof(char))) == NULL)
        return (char *)ERROR_PTR("dest not made", __func__, NULL);
    npre = ptr - src;
    memcpy(dest, src, npre);
    strcpy(dest + npre, sub2);
    strcpy(dest + npre + nsub2, ptr + nsub1);
    if (ploc) *ploc = npre + nsub2;
    return dest;
}


/*!
 * \brief   stringFindEachSubstr()
 *
 * \param[in]    src        input string; can be of zero length
 * \param[in]    sub        substring to be searched for
 * \return  dna of offsets where the sequence is found, or NULL if
 *              none are found or on error
 *
 * <pre>
 * Notes:
 *      (1) This finds every non-overlapping occurrence in %src of %sub.
 *          After it finds each match, it moves forward in %src by the length
 *          of %sub before continuing the search.  So for example,
 *          if you search for the sequence 'aa' in the data 'baaabbb',
 *          you find one match at position 1.

 * </pre>
 */
L_DNA *
stringFindEachSubstr(const char  *src,
                     const char  *sub)
{
    if (!src || !sub)
        return (L_DNA *)ERROR_PTR("src, sub not both defined", __func__, NULL);

    return arrayFindEachSequence((const l_uint8 *)src, strlen(src),
                                 (const l_uint8 *)sub, strlen(sub));
}


/*!
 * \brief   stringFindSubstr()
 *
 * \param[in]    src     input string; can be of zero length
 * \param[in]    sub     substring to be searched for; must not be empty
 * \param[out]   ploc    [optional] location of substring in src
 * \return  1 if found; 0 if not found or on error
 *
 * <pre>
 * Notes:
 *      (1) This is a wrapper around strstr().  It finds the first
 *          instance of %sub in %src.  If the substring is not found
 *          and the location is returned, it has the value -1.
 *      (2) Both %src and %sub must be defined, and %sub must have
 *          length of at least 1.
 * </pre>
 */
l_int32
stringFindSubstr(const char  *src,
                 const char  *sub,
                 l_int32     *ploc)
{
const char *ptr;

    if (ploc) *ploc = -1;
    if (!src || !sub)
        return ERROR_INT("src and sub not both defined", __func__, 0);
    if (strlen(sub) == 0)
        return ERROR_INT("substring length 0", __func__, 0);
    if (strlen(src) == 0)
        return 0;

    if ((ptr = strstr(src, sub)) == NULL)  /* not found */
        return 0;

    if (ploc)
        *ploc = ptr - src;
    return 1;
}


/*!
 * \brief   arrayReplaceEachSequence()
 *
 * \param[in]    datas       source byte array
 * \param[in]    dataslen    length of source data, in bytes
 * \param[in]    seq         subarray of bytes to find in source data
 * \param[in]    seqlen      length of subarray, in bytes
 * \param[in]    newseq      replacement subarray; can be null
 * \param[in]    newseqlen   length of replacement subarray, in bytes
 * \param[out]   pdatadlen   length of dest byte array, in bytes
 * \param[out]   pcount      [optional] the number of times that sub1
 *                           is found in src; 0 if not found
 * \return  datad   with all all subarrays replaced (or removed)
 *
 * <pre>
 * Notes:
 *      (1) The byte arrays %datas, %seq and %newseq are not C strings,
 *          because they can contain null bytes.  Therefore, for each
 *          we must give the length of the array.
 *      (2) If %newseq == NULL, this just removes all instances of %seq.
 *          Otherwise, it replaces every non-overlapping occurrence of
 *          %seq in %datas with %newseq. A new array %datad and its
 *          size are returned.  See arrayFindEachSequence() for more
 *          details on finding non-overlapping occurrences.
 *      (3) If no instances of %seq are found, this returns a copy of %datas.
 *      (4) The returned %datad is null terminated.
 *      (5) Can use stringReplaceEachSubstr() if using C strings.
 * </pre>
 */
l_uint8 *
arrayReplaceEachSequence(const l_uint8  *datas,
                         size_t          dataslen,
                         const l_uint8  *seq,
                         size_t          seqlen,
                         const l_uint8  *newseq,
                         size_t          newseqlen,
                         size_t         *pdatadlen,
                         l_int32        *pcount)
{
l_uint8  *datad;
size_t    newsize;
l_int32   n, i, j, di, si, index, incr;
L_DNA    *da;

    if (pcount) *pcount = 0;
    if (!datas || !seq)
        return (l_uint8 *)ERROR_PTR("datas & seq not both defined",
                                    __func__, NULL);
    if (!pdatadlen)
        return (l_uint8 *)ERROR_PTR("&datadlen not defined", __func__, NULL);
    *pdatadlen = 0;

        /* Identify the locations of the sequence.  If there are none,
         * return a copy of %datas. */
    if ((da = arrayFindEachSequence(datas, dataslen, seq, seqlen)) == NULL) {
        *pdatadlen = dataslen;
        return l_binaryCopy(datas, dataslen);
    }

        /* Allocate the output data; insure null termination */
    n = l_dnaGetCount(da);
    if (pcount) *pcount = n;
    if (!newseq) newseqlen = 0;
    newsize = dataslen + n * (newseqlen - seqlen) + 4;
    if ((datad = (l_uint8 *)LEPT_CALLOC(newsize, sizeof(l_uint8))) == NULL) {
        l_dnaDestroy(&da);
        return (l_uint8 *)ERROR_PTR("datad not made", __func__, NULL);
    }

        /* Replace each sequence instance with a new sequence */
    l_dnaGetIValue(da, 0, &si);
    for (i = 0, di = 0, index = 0; i < dataslen; i++) {
        if (i == si) {
            index++;
            if (index < n) {
                l_dnaGetIValue(da, index, &si);
                incr = L_MIN(seqlen, si - i);  /* amount to remove from datas */
            } else {
                incr = seqlen;
            }
            i += incr - 1;  /* jump over the matched sequence in datas */
            if (newseq) {  /* add new sequence to datad */
                for (j = 0; j < newseqlen; j++)
                    datad[di++] = newseq[j];
            }
        } else {
            datad[di++] = datas[i];
        }
    }

    *pdatadlen = di;
    l_dnaDestroy(&da);
    return datad;
}


/*!
 * \brief   arrayFindEachSequence()
 *
 * \param[in]    data       byte array
 * \param[in]    datalen    length of data, in bytes
 * \param[in]    sequence   subarray of bytes to find in data
 * \param[in]    seqlen     length of sequence, in bytes
 * \return  dna of offsets where the sequence is found, or NULL if
 *              none are found or on error
 *
 * <pre>
 * Notes:
 *      (1) The byte arrays %data and %sequence are not C strings,
 *          because they can contain null bytes.  Therefore, for each
 *          we must give the length of the array.
 *      (2) This finds every non-overlapping occurrence in %data of %sequence.
 *          After it finds each match, it moves forward by the length
 *          of the sequence before continuing the search.  So for example,
 *          if you search for the sequence 'aa' in the data 'baaabbb',
 *          you find one match at position 1.
 * </pre>
 */
L_DNA *
arrayFindEachSequence(const l_uint8  *data,
                      size_t          datalen,
                      const l_uint8  *sequence,
                      size_t          seqlen)
{
l_int32  start, offset, realoffset, found;
L_DNA   *da;

    if (!data || !sequence)
        return (L_DNA *)ERROR_PTR("data & sequence not both defined",
                                  __func__, NULL);

    da = l_dnaCreate(0);
    start = 0;
    while (1) {
        arrayFindSequence(data + start, datalen - start, sequence, seqlen,
                          &offset, &found);
        if (found == FALSE)
            break;

        realoffset = start + offset;
        l_dnaAddNumber(da, realoffset);
        start = realoffset + seqlen;
        if (start >= datalen)
            break;
    }

    if (l_dnaGetCount(da) == 0)
        l_dnaDestroy(&da);
    return da;
}


/*!
 * \brief   arrayFindSequence()
 *
 * \param[in]    data       byte array
 * \param[in]    datalen    length of data, in bytes
 * \param[in]    sequence   subarray of bytes to find in data
 * \param[in]    seqlen     length of sequence, in bytes
 * \param[out]   poffset    offset from beginning of
 *                          data where the sequence begins
 * \param[out]   pfound     1 if sequence is found; 0 otherwise
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The byte arrays 'data' and 'sequence' are in general not C strings,
 *          because they can contain null bytes.  Therefore, for each
 *          we must give the length of the array.
 *      (2) This searches for the first occurrence in %data of %sequence,
 *          which consists of %seqlen bytes.  The parameter %seqlen
 *          must not exceed the actual length of the %sequence byte array.
 *      (3) If either byte array is a C string, cast the array to
 *          (const l_uint8 *) and use strlen() on the string for its length.
 *      (4) If the sequence is not found, the offset will be 0, so you
 *          must check %found.
 * </pre>
 */
l_ok
arrayFindSequence(const l_uint8  *data,
                  size_t          datalen,
                  const l_uint8  *sequence,
                  size_t          seqlen,
                  l_int32        *poffset,
                  l_int32        *pfound)
{
l_int32  i, j, found, lastpos;

    if (poffset) *poffset = 0;
    if (pfound) *pfound = FALSE;
    if (!data || !sequence)
        return ERROR_INT("data & sequence not both defined", __func__, 1);
    if (!poffset || !pfound)
        return ERROR_INT("&offset and &found not defined", __func__, 1);

    lastpos = datalen - seqlen + 1;
    found = FALSE;
    for (i = 0; i < lastpos; i++) {
        for (j = 0; j < seqlen; j++) {
            if (data[i + j] != sequence[j])
                 break;
            if (j == seqlen - 1)
                 found = TRUE;
        }
        if (found == TRUE)
            break;
    }

    if (found == TRUE) {
        *poffset = i;
        *pfound = TRUE;
    }
    return 0;
}


/*--------------------------------------------------------------------*
 *                             Safe realloc                           *
 *--------------------------------------------------------------------*/
/*!
 * \brief   reallocNew()
 *
 * \param[in,out]  pindata    nulls indata before reallocing
 * \param[in]      oldsize    size of input data to be copied, in bytes
 * \param[in]      newsize    size of buffer to be reallocated in bytes
 * \return  ptr to new data, or NULL on error
 *
 *  Action: !N.B. 3) and (4!
 *      1 Allocates memory, initialized to 0
 *      2 Copies as much of the input data as possible
 *          to the new block, truncating the copy if necessary
 *      3 Frees the input data
 *      4 Zeroes the input data ptr
 *
 * <pre>
 * Notes:
 *      (1) If newsize == 0, frees input data and nulls ptr
 *      (2) If input data is null, only callocs new memory
 *      (3) This differs from realloc in that it always allocates
 *          new memory (if newsize > 0) and initializes it to 0,
 *          it requires the amount of old data to be copied,
 *          and it takes the address of the input ptr and
 *          nulls the handle.
 * </pre>
 */
void *
reallocNew(void  **pindata,
           size_t  oldsize,
           size_t  newsize)
{
size_t   minsize;
void    *indata;
void    *newdata;

    if (!pindata)
        return ERROR_PTR("input data not defined", __func__, NULL);
    indata = *pindata;

    if (newsize == 0) {   /* nonstandard usage */
        if (indata) {
            LEPT_FREE(indata);
            *pindata = NULL;
        }
        return NULL;
    }

    if (!indata) {  /* nonstandard usage */
        if ((newdata = (void *)LEPT_CALLOC(1, newsize)) == NULL)
            return ERROR_PTR("newdata not made", __func__, NULL);
        return newdata;
    }

        /* Standard usage */
    if ((newdata = (void *)LEPT_CALLOC(1, newsize)) == NULL)
        return ERROR_PTR("newdata not made", __func__, NULL);
    minsize = L_MIN(oldsize, newsize);
    memcpy(newdata, indata, minsize);
    LEPT_FREE(indata);
    *pindata = NULL;
    return newdata;
}


/*--------------------------------------------------------------------*
 *                 Read and write between file and memory             *
 *--------------------------------------------------------------------*/
/*!
 * \brief   l_binaryRead()
 *
 * \param[in]    filename
 * \param[out]   pnbytes    number of bytes read
 * \return  data, or NULL on error
 */
l_uint8 *
l_binaryRead(const char  *filename,
             size_t      *pnbytes)
{
l_uint8  *data;
FILE     *fp;

    if (!pnbytes)
        return (l_uint8 *)ERROR_PTR("pnbytes not defined", __func__, NULL);
    *pnbytes = 0;
    if (!filename)
        return (l_uint8 *)ERROR_PTR("filename not defined", __func__, NULL);

    if ((fp = fopenReadStream(filename)) == NULL)
        return (l_uint8 *)ERROR_PTR_1("file stream not opened",
                                      filename, __func__, NULL);
    data = l_binaryReadStream(fp, pnbytes);
    fclose(fp);
    return data;
}


/*!
 * \brief   l_binaryReadStream()
 *
 * \param[in]    fp        file stream opened to read; can be stdin
 * \param[out]   pnbytes   number of bytes read
 * \return  null-terminated array, or NULL on error; reading 0 bytes
 *          is not an error
 *
 * <pre>
 * Notes:
 *      (1) The returned array is terminated with a null byte so that it can
 *          be used to read ascii data from a file into a proper C string.
 *      (2) This can be used to capture data that is piped in via stdin,
 *          because it does not require seeking within the file.
 *      (3) For example, you can read an image from stdin into memory
 *          using shell redirection, with one of these shell commands:
 * \code
 *             cat <imagefile> | readprog
 *             readprog < <imagefile>
 * \endcode
 *          where readprog is:
 * \code
 *             l_uint8 *data = l_binaryReadStream(stdin, &nbytes);
 *             Pix *pix = pixReadMem(data, nbytes);
 * \endcode
 * </pre>
 */
l_uint8 *
l_binaryReadStream(FILE    *fp,
                   size_t  *pnbytes)
{
l_uint8    *data;
l_int32     seekable, navail, nadd, nread;
L_BBUFFER  *bb;

    if (!pnbytes)
        return (l_uint8 *)ERROR_PTR("&nbytes not defined", __func__, NULL);
    *pnbytes = 0;
    if (!fp)
        return (l_uint8 *)ERROR_PTR("fp not defined", __func__, NULL);

        /* Test if the stream is seekable, by attempting to seek to
         * the start of data.  This is a no-op.  If it is seekable, use
         * l_binaryReadSelectStream() to determine the size of the
         * data to be read in advance. */
    seekable = (ftell(fp) == 0) ? 1 : 0;
    if (seekable)
        return l_binaryReadSelectStream(fp, 0, 0, pnbytes);

        /* If it is not seekable, use the bbuffer to realloc memory
         * as needed during reading. */
    bb = bbufferCreate(NULL, 4096);
    while (1) {
        navail = bb->nalloc - bb->n;
        if (navail < 4096) {
             nadd = L_MAX(bb->nalloc, 4096);
             bbufferExtendArray(bb, nadd);
        }
        nread = fread((void *)(bb->array + bb->n), 1, 4096, fp);
        bb->n += nread;
        if (nread != 4096) break;
    }

        /* Copy the data to a new array sized for the data, because
         * the bbuffer array can be nearly twice the size we need. */
    if ((data = (l_uint8 *)LEPT_CALLOC(bb->n + 1, sizeof(l_uint8))) != NULL) {
        memcpy(data, bb->array, bb->n);
        *pnbytes = bb->n;
    } else {
        L_ERROR("calloc fail for data\n", __func__);
    }

    bbufferDestroy(&bb);
    return data;
}


/*!
 * \brief   l_binaryReadSelect()
 *
 * \param[in]    filename
 * \param[in]    start     first byte to read
 * \param[in]    nbytes    number of bytes to read; use 0 to read to end of file
 * \param[out]   pnread    number of bytes actually read
 * \return  data, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The returned array is terminated with a null byte so that it can
 *          be used to read ascii data from a file into a proper C string.
 * </pre>
 */
l_uint8 *
l_binaryReadSelect(const char  *filename,
                   size_t       start,
                   size_t       nbytes,
                   size_t      *pnread)
{
l_uint8  *data;
FILE     *fp;

    if (!pnread)
        return (l_uint8 *)ERROR_PTR("pnread not defined", __func__, NULL);
    *pnread = 0;
    if (!filename)
        return (l_uint8 *)ERROR_PTR("filename not defined", __func__, NULL);

    if ((fp = fopenReadStream(filename)) == NULL)
        return (l_uint8 *)ERROR_PTR_1("file stream not opened",
                                      filename, __func__, NULL);
    data = l_binaryReadSelectStream(fp, start, nbytes, pnread);
    fclose(fp);
    return data;
}


/*!
 * \brief   l_binaryReadSelectStream()
 *
 * \param[in]    fp       file stream
 * \param[in]    start    first byte to read
 * \param[in]    nbytes   number of bytes to read; use 0 to read to end of file
 * \param[out]   pnread   number of bytes actually read
 * \return  null-terminated array, or NULL on error; reading 0 bytes
 *          is not an error
 *
 * <pre>
 * Notes:
 *      (1) The returned array is terminated with a null byte so that it can
 *          be used to read ascii data from a file into a proper C string.
 *          If the file to be read is empty and %start == 0, an array
 *          with a single null byte is returned.
 *      (2) Side effect: the stream pointer is re-positioned to the
 *          beginning of the file.
 * </pre>
 */
l_uint8 *
l_binaryReadSelectStream(FILE    *fp,
                         size_t   start,
                         size_t   nbytes,
                         size_t  *pnread)
{
l_uint8  *data;
size_t    bytesleft, bytestoread, nread, filebytes;

    if (!pnread)
        return (l_uint8 *)ERROR_PTR("&nread not defined", __func__, NULL);
    *pnread = 0;
    if (!fp)
        return (l_uint8 *)ERROR_PTR("stream not defined", __func__, NULL);

        /* Verify and adjust the parameters if necessary */
    fseek(fp, 0, SEEK_END);  /* EOF */
    filebytes = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (start > filebytes) {
        L_ERROR("start = %zu but filebytes = %zu\n", __func__,
                start, filebytes);
        return NULL;
    }
    if (filebytes == 0)  /* start == 0; nothing to read; return null byte */
        return (l_uint8 *)LEPT_CALLOC(1, 1);
    bytesleft = filebytes - start;  /* greater than 0 */
    if (nbytes == 0) nbytes = bytesleft;
    bytestoread = (bytesleft >= nbytes) ? nbytes : bytesleft;

        /* Read the data */
    if ((data = (l_uint8 *)LEPT_CALLOC(1, bytestoread + 1)) == NULL)
        return (l_uint8 *)ERROR_PTR("calloc fail for data", __func__, NULL);
    fseek(fp, start, SEEK_SET);
    nread = fread(data, 1, bytestoread, fp);
    if (nbytes != nread)
        L_INFO("%zu bytes requested; %zu bytes read\n", __func__,
               nbytes, nread);
    *pnread = nread;
    fseek(fp, 0, SEEK_SET);
    return data;
}


/*!
 * \brief   l_binaryWrite()
 *
 * \param[in]    filename     output file
 * \param[in]    operation    "w" for write; "a" for append
 * \param[in]    data         binary data to be written
 * \param[in]    nbytes       size of data array
 * \return  0 if OK; 1 on error
 */
l_ok
l_binaryWrite(const char  *filename,
              const char  *operation,
              const void  *data,
              size_t       nbytes)
{
char   actualOperation[20];
FILE  *fp;

    if (!filename)
        return ERROR_INT("filename not defined", __func__, 1);
    if (!operation)
        return ERROR_INT("operation not defined", __func__, 1);
    if (!data)
        return ERROR_INT("data not defined", __func__, 1);
    if (nbytes <= 0)
        return ERROR_INT("nbytes must be > 0", __func__, 1);

    if (strcmp(operation, "w") && strcmp(operation, "a"))
        return ERROR_INT("operation not one of {'w','a'}", __func__, 1);

        /* The 'b' flag to fopen() is ignored for all POSIX
         * conforming systems.  However, Windows needs the 'b' flag. */
    stringCopy(actualOperation, operation, 2);
    stringCat(actualOperation, 20, "b");

    if ((fp = fopenWriteStream(filename, actualOperation)) == NULL)
        return ERROR_INT_1("stream not opened", filename, __func__, 1);
    fwrite(data, 1, nbytes, fp);
    fclose(fp);
    return 0;
}


/*!
 * \brief   nbytesInFile()
 *
 * \param[in]    filename
 * \return  nbytes in file; 0 on error
 */
size_t
nbytesInFile(const char  *filename)
{
size_t  nbytes;
FILE   *fp;

    if (!filename)
        return ERROR_INT("filename not defined", __func__, 0);
    if ((fp = fopenReadStream(filename)) == NULL)
        return ERROR_INT_1("stream not opened", filename, __func__, 0);
    nbytes = fnbytesInFile(fp);
    fclose(fp);
    return nbytes;
}


/*!
 * \brief   fnbytesInFile()
 *
 * \param[in]    fp    file stream
 * \return  nbytes in file; 0 on error
 */
size_t
fnbytesInFile(FILE  *fp)
{
l_int64  pos, nbytes;

    if (!fp)
        return ERROR_INT("stream not open", __func__, 0);

    pos = ftell(fp);          /* initial position */
    if (pos < 0)
        return ERROR_INT("seek position must be > 0", __func__, 0);
    fseek(fp, 0, SEEK_END);   /* EOF */
    nbytes = ftell(fp);
    if (nbytes < 0)
        return ERROR_INT("nbytes is < 0", __func__, 0);
    fseek(fp, pos, SEEK_SET);        /* back to initial position */
    return nbytes;
}


/*--------------------------------------------------------------------*
 *                     Copy and compare in memory                     *
 *--------------------------------------------------------------------*/
/*!
 * \brief   l_binaryCopy()
 *
 * \param[in]    datas
 * \param[in]    size    of data array
 * \return  datad on heap, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) We add 4 bytes to the zeroed output because in some cases
 *          (e.g., string handling) it is important to have the data
 *          be null terminated.  This guarantees that after the memcpy,
 *          the result is automatically null terminated.
 * </pre>
 */
l_uint8 *
l_binaryCopy(const l_uint8  *datas,
             size_t          size)
{
l_uint8  *datad;

    if (!datas)
        return (l_uint8 *)ERROR_PTR("datas not defined", __func__, NULL);

    if ((datad = (l_uint8 *)LEPT_CALLOC(size + 4, sizeof(l_uint8))) == NULL)
        return (l_uint8 *)ERROR_PTR("datad not made", __func__, NULL);
    memcpy(datad, datas, size);
    return datad;
}


/*!
 * \brief   l_binaryCompare()
 *
 * \param[in]    data1
 * \param[in]    size1   of data1
 * \param[in]    data2
 * \param[in]    size2   of data1
 * \param[out]   psame  (1 if the same, 0 if different)
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This can also be used to compare C strings str1 and str2.
 *          If the string lengths are not known, use strlen():
 *            l_binaryCompare((l_uint8 *)str1, strlen(str1),
                              (l_uint8 *)str2, strlen(str2));
 * </pre>
 */
l_ok
l_binaryCompare(const l_uint8  *data1,
                size_t          size1,
                const l_uint8  *data2,
                size_t          size2,
                l_int32        *psame)
{
l_int32  i;

    if (!psame)
        return ERROR_INT("&same not defined", __func__, 1);
    *psame = FALSE;
    if (!data1 || !data2)
        return ERROR_INT("data1 and data2 not both defined", __func__, 1);
    if (size1 != size2) return 0;
    for (i = 0; i < size1; i++) {
        if (data1[i] != data2[i])
            return 0;
    }
    *psame = TRUE;
    return 0;
}


/*--------------------------------------------------------------------*
 *                         File copy operations                       *
 *--------------------------------------------------------------------*/
/*!
 * \brief   fileCopy()
 *
 * \param[in]    srcfile   copy from this file
 * \param[in]    newfile   copy to this file
 * \return  0 if OK, 1 on error
 */
l_ok
fileCopy(const char  *srcfile,
         const char  *newfile)
{
l_int32   ret;
size_t    nbytes;
l_uint8  *data;

    if (!srcfile)
        return ERROR_INT("srcfile not defined", __func__, 1);
    if (!newfile)
        return ERROR_INT("newfile not defined", __func__, 1);

    if ((data = l_binaryRead(srcfile, &nbytes)) == NULL)
        return ERROR_INT("data not returned", __func__, 1);
    ret = l_binaryWrite(newfile, "w", data, nbytes);
    LEPT_FREE(data);
    return ret;
}


/*!
 * \brief   fileConcatenate()
 *
 * \param[in]    srcfile   append data from this file
 * \param[in]    destfile  add data to this file
 * \return  0 if OK, 1 on error
 */
l_ok
fileConcatenate(const char  *srcfile,
                const char  *destfile)
{
size_t    nbytes;
l_uint8  *data;

    if (!srcfile)
        return ERROR_INT("srcfile not defined", __func__, 1);
    if (!destfile)
        return ERROR_INT("destfile not defined", __func__, 1);

    data = l_binaryRead(srcfile, &nbytes);
    l_binaryWrite(destfile, "a", data, nbytes);
    LEPT_FREE(data);
    return 0;
}


/*!
 * \brief   fileAppendString()
 *
 * \param[in]    filename
 * \param[in]    str       string to append to file
 * \return  0 if OK, 1 on error
 */
l_ok
fileAppendString(const char  *filename,
                 const char  *str)
{
FILE  *fp;

    if (!filename)
        return ERROR_INT("filename not defined", __func__, 1);
    if (!str)
        return ERROR_INT("str not defined", __func__, 1);

    if ((fp = fopenWriteStream(filename, "a")) == NULL)
        return ERROR_INT_1("stream not opened", filename, __func__, 1);
    fprintf(fp, "%s", str);
    fclose(fp);
    return 0;
}


/*--------------------------------------------------------------------*
 *                         File split operations                      *
 *--------------------------------------------------------------------*/
/*!
 * \brief   fileSplitLinesUniform()
 *
 * \param[in]    filename      input file
 * \param[in]    n             number of output files (>= 1)
 * \param[in]    save_empty    1 to save empty lines; 0 to remove them
 * \param[in]    rootpath      root pathname of output files
 * \param[in]    ext           output extension, including the '.'; can be NULL
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This splits an input text file into %n files with roughly
 *          equal numbers of text lines in each file.
 *      (2) if %save_empty == 1, empty lines are included, and concatention
 *          of the text in the split files will be identical to the original.
 *      (3) The output filenames are in the form:
 *               <rootpath>_N.<ext>, N = 1, ... n
 *      (4) This handles the temp directory pathname conversion where needed:
 *              /tmp  ==>  [OS specific temp directory]
 *      (5) Files can also be sharded into sets of lines by the program 'split':
 *              split -n l/<n> <filename>
 *          Using 'split', the resulting files have approximately equal
 *          numbers of bytes, rather than equal numbers of lines.
 * </pre>
 */
l_ok
fileSplitLinesUniform(const char  *filename,
                      l_int32      n,
                      l_int32      save_empty,
                      const char  *rootpath,
                      const char  *ext)
{
l_int32   i, totlines, nlines, index;
size_t    nbytes;
l_uint8  *data;
char     *str;
char      outname[512];
NUMA     *na;
SARRAY   *sa;

    if (!filename)
        return ERROR_INT("filename not defined", __func__, 1);
    if (!rootpath)
        return ERROR_INT("rootpath not defined", __func__, 1);
    if (n <= 0)
        return ERROR_INT("n must be > 0", __func__, 1);
    if (save_empty != 0 && save_empty != 1)
        return ERROR_INT("save_empty not 0 or 1", __func__, 1);

        /* Make sarray of lines; the newlines are stripped off */
    if ((data = l_binaryRead(filename, &nbytes)) == NULL)
        return ERROR_INT("data not read", __func__, 1);
    sa = sarrayCreateLinesFromString((const char *)data, save_empty);
    LEPT_FREE(data);
    if (!sa)
        return ERROR_INT("sa not made", __func__, 1);
    totlines = sarrayGetCount(sa);
    if (n > totlines) {
        sarrayDestroy(&sa);
        L_ERROR("num files = %d > num lines = %d\n", __func__, n, totlines);
        return 1;
    }

        /* Write n sets of lines to n files, adding the newlines back */
    na = numaGetUniformBinSizes(totlines, n);
    index = 0;
    for (i = 0; i < n; i++) {
        if (ext == NULL)
            snprintf(outname, sizeof(outname), "%s_%d", rootpath, i);
        else
            snprintf(outname, sizeof(outname), "%s_%d%s", rootpath, i, ext);
        numaGetIValue(na, i, &nlines);
        str = sarrayToStringRange(sa, index, nlines, 1);  /* add newlines */
        l_binaryWrite(outname, "w", str, strlen(str));
        LEPT_FREE(str);
        index += nlines;
    }
    numaDestroy(&na);
    sarrayDestroy(&sa);
    return 0;
}


/*--------------------------------------------------------------------*
 *          Multi-platform functions for opening file streams         *
 *--------------------------------------------------------------------*/
/*!
 * \brief   fopenReadStream()
 *
 * \param[in]    filename
 * \return  stream, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This should be used whenever you want to run fopen() to
 *          read from a stream.  Never call fopen() directory.
 *      (2) This handles the temp directory pathname conversion where needed:
 *              /tmp  ==>  [OS specific temp directory]
 * </pre>
 */
FILE *
fopenReadStream(const char  *filename)
{
char  *fname, *tail;
FILE  *fp;

    if (!filename)
        return (FILE *)ERROR_PTR("filename not defined", __func__, NULL);

        /* Try input filename */
    fname = genPathname(filename, NULL);
    fp = fopen(fname, "rb");
    LEPT_FREE(fname);
    if (fp) return fp;

        /* Else, strip directory and try locally */
    splitPathAtDirectory(filename, NULL, &tail);
    if (!tail)
        return (FILE*)ERROR_PTR_1("tail not found", filename, __func__, NULL);
    fp = fopen(tail, "rb");
    if (!fp)
        L_ERROR("failed to open locally with tail %s for filename %s\n",
                __func__, tail, filename);
    LEPT_FREE(tail);
    return fp;
}


/*!
 * \brief   fopenWriteStream()
 *
 * \param[in]    filename
 * \param[in]    modestring
 * \return  stream, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This should be used whenever you want to run fopen() to
 *          write or append to a stream.  Never call fopen() directory.
 *      (2) This handles the temp directory pathname conversion where needed:
 *              /tmp  ==>  [OS specific temp directory]
 * </pre>
 */
FILE *
fopenWriteStream(const char  *filename,
                 const char  *modestring)
{
char  *fname;
FILE  *fp;

    if (!filename)
        return (FILE *)ERROR_PTR("filename not defined", __func__, NULL);

    fname = genPathname(filename, NULL);
    fp = fopen(fname, modestring);
    if (!fp)
        fp = (FILE *)ERROR_PTR_1("stream not opened", fname, __func__, NULL);
    LEPT_FREE(fname);
    return fp;
}


/*!
 * \brief   fopenReadFromMemory()
 *
 * \param[in]    data, size
 * \return  file stream, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Work-around if fmemopen() not available.
 *      (2) Windows tmpfile() writes into the root C:\ directory, which
 *          requires admin privileges.  This also works around that.
 * </pre>
 */
FILE *
fopenReadFromMemory(const l_uint8  *data,
                    size_t          size)
{
FILE  *fp;

    if (!data)
        return (FILE *)ERROR_PTR("data not defined", __func__, NULL);

#if HAVE_FMEMOPEN
    if ((fp = fmemopen((void *)data, size, "rb")) == NULL)
        return (FILE *)ERROR_PTR("stream not opened", __func__, NULL);
#else  /* write to tmp file */
    L_INFO("no fmemopen API --> work-around: write to temp file\n", __func__);
  #ifdef _WIN32
    if ((fp = fopenWriteWinTempfile()) == NULL)
        return (FILE *)ERROR_PTR("tmpfile stream not opened", __func__, NULL);
  #else
    if ((fp = tmpfile()) == NULL)
        return (FILE *)ERROR_PTR("tmpfile stream not opened", __func__, NULL);
  #endif  /*  _WIN32 */
    fwrite(data, 1, size, fp);
    rewind(fp);
#endif  /* HAVE_FMEMOPEN */

    return fp;
}


/*--------------------------------------------------------------------*
 *                Opening a Windows tmpfile for writing               *
 *--------------------------------------------------------------------*/
/*!
 * \brief   fopenWriteWinTempfile()
 *
 * \return  file stream, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The Windows version of tmpfile() writes into the root
 *          C:\ directory, which requires admin privileges.  This
 *          function provides an alternative implementation.
 * </pre>
 */
FILE *
fopenWriteWinTempfile(void)
{
#ifdef _WIN32
l_int32  handle;
FILE    *fp;
char    *filename;

    if ((filename = l_makeTempFilename()) == NULL) {
        L_ERROR("l_makeTempFilename failed, %s\n", __func__, strerror(errno));
        return NULL;
    }

    handle = _open(filename, _O_CREAT | _O_RDWR | _O_SHORT_LIVED |
                   _O_TEMPORARY | _O_BINARY, _S_IREAD | _S_IWRITE);
    lept_free(filename);
    if (handle == -1) {
        L_ERROR("_open failed, %s\n", __func__, strerror(errno));
        return NULL;
    }

    if ((fp = _fdopen(handle, "r+b")) == NULL) {
        L_ERROR("_fdopen failed, %s\n", __func__, strerror(errno));
        return NULL;
    }

    return fp;
#else
    return NULL;
#endif  /*  _WIN32 */
}


/*--------------------------------------------------------------------*
 *       Multi-platform functions that avoid C-runtime boundary       *
 *             crossing for applications with Windows DLLs            *
 *--------------------------------------------------------------------*/
/*
 *  Problems arise when pointers to streams and data are passed
 *  between two Windows DLLs that have been generated with different
 *  C runtimes.  To avoid this, leptonica provides wrappers for
 *  several C library calls.
 */
/*!
 * \brief   lept_fopen()
 *
 * \param[in]    filename
 * \param[in]    mode       same as for fopen(); e.g., "rb"
 * \return  stream or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This must be used by any application that passes
 *          a file handle to a leptonica Windows DLL.
 * </pre>
 */
FILE *
lept_fopen(const char  *filename,
           const char  *mode)
{
    if (!filename)
        return (FILE *)ERROR_PTR("filename not defined", __func__, NULL);
    if (!mode)
        return (FILE *)ERROR_PTR("mode not defined", __func__, NULL);

    if (stringFindSubstr(mode, "r", NULL))
        return fopenReadStream(filename);
    else
        return fopenWriteStream(filename, mode);
}


/*!
 * \brief   lept_fclose()
 *
 * \param[in]    fp    file stream
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This should be used by any application that accepts
 *          a file handle generated by a leptonica Windows DLL.
 * </pre>
 */
l_ok
lept_fclose(FILE *fp)
{
    if (!fp)
        return ERROR_INT("stream not defined", __func__, 1);

    return fclose(fp);
}


/*!
 * \brief   lept_calloc()
 *
 * \param[in]    nmemb    number of members
 * \param[in]    size     of each member
 * \return  void ptr, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) For safety with Windows DLLs, this can be used in conjunction
 *          with lept_free() to avoid C-runtime boundary problems.
 *          Just use these two functions throughout your application.
 * </pre>
 */
void *
lept_calloc(size_t  nmemb,
            size_t  size)
{
    if (nmemb <= 0 || size <= 0)
        return NULL;
    return LEPT_CALLOC(nmemb, size);
}


/*!
 * \brief   lept_free()
 *
 * \param[in]    ptr
 *
 * <pre>
 * Notes:
 *      (1) This should be used by any application that accepts
 *          heap data allocated by a leptonica Windows DLL.
 * </pre>
 */
void
lept_free(void *ptr)
{
    if (!ptr) return;
    LEPT_FREE(ptr);
}


/*--------------------------------------------------------------------*
 *                Multi-platform file system operations               *
 *         [ These only write to /tmp or its subdirectories ]         *
 *--------------------------------------------------------------------*/
/*!
 * \brief   lept_mkdir()
 *
 * \param[in]    subdir    of /tmp or its OS specific equivalent
 * \return  0 on success, non-zero on failure
 *
 * <pre>
 * Notes:
 *      (1) %subdir is a partial path that can consist of one or more
 *          directories.
 *      (2) This makes any subdirectories of /tmp that are required.
 *      (3) The root temp directory is:
 *            /tmp    (unix)  [default]
 *            [Temp]  (Windows)
 * </pre>
 */
l_int32
lept_mkdir(const char  *subdir)
{
char     *dir, *tmpdir;
l_int32   i, n;
l_int32   ret = 0;
SARRAY   *sa;
#ifdef  _WIN32
l_uint32  attributes;
#endif  /* _WIN32 */

    if (!LeptDebugOK) {
        L_INFO("making named temp subdirectory %s is disabled\n",
               __func__, subdir);
        return 0;
    }

    if (!subdir)
        return ERROR_INT("subdir not defined", __func__, 1);
    if ((strlen(subdir) == 0) || (subdir[0] == '.') || (subdir[0] == '/'))
        return ERROR_INT("subdir not an actual subdirectory", __func__, 1);

    sa = sarrayCreate(0);
    sarraySplitString(sa, subdir, "/");
    n = sarrayGetCount(sa);
    dir = genPathname("/tmp", NULL);
    ret = 0;   /* don't check ret values with unix because if a directory
                * exists, mkdir() returns -1  */
       /* Make sure the tmp directory exists */
#ifndef _WIN32
    mkdir(dir, 0777);
#else
    attributes = GetFileAttributesA(dir);
    if (attributes == INVALID_FILE_ATTRIBUTES)
        ret = (CreateDirectoryA(dir, NULL) ? 0 : 1);
#endif
        /* Make all the subdirectories */
    for (i = 0; i < n; i++) {
        tmpdir = pathJoin(dir, sarrayGetString(sa, i, L_NOCOPY));
#ifndef _WIN32
        mkdir(tmpdir, 0777);
#else
        if (CreateDirectoryA(tmpdir, NULL) == 0)
            ret += (GetLastError() != ERROR_ALREADY_EXISTS);
#endif
        LEPT_FREE(dir);
        dir = tmpdir;
    }
    LEPT_FREE(dir);
    sarrayDestroy(&sa);
    if (ret > 0)
        L_ERROR("failure to create %d directories\n", __func__, ret);
    return ret;
}


/*!
 * \brief   lept_rmdir()
 *
 * \param[in]    subdir    of /tmp or its OS specific equivalent
 * \return  0 on success, non-zero on failure
 *
 * <pre>
 * Notes:
 *      (1) %subdir is a partial path that can consist of one or more
 *          directories.
 *      (2) This removes all files from the specified subdirectory of
 *          the root temp directory:
 *            /tmp    (unix)
 *            [Temp]  (Windows)
 *          and then removes the subdirectory.
 *      (3) The combination
 *            lept_rmdir(subdir);
 *            lept_mkdir(subdir);
 *          is guaranteed to give you an empty subdirectory.
 * </pre>
 */
l_int32
lept_rmdir(const char  *subdir)
{
char    *dir, *fname, *fullname;
l_int32  exists, ret, i, nfiles;
SARRAY  *sa;
#ifdef _WIN32
char    *newpath;
#else
char    *realdir;
#endif  /* _WIN32 */

    if (!subdir)
        return ERROR_INT("subdir not defined", __func__, 1);
    if ((strlen(subdir) == 0) || (subdir[0] == '.') || (subdir[0] == '/'))
        return ERROR_INT("subdir not an actual subdirectory", __func__, 1);

        /* Find the temp subdirectory */
    dir = pathJoin("/tmp", subdir);
    if (!dir)
        return ERROR_INT("directory name not made", __func__, 1);
    lept_direxists(dir, &exists);
    if (!exists) {  /* fail silently */
        LEPT_FREE(dir);
        return 0;
    }

        /* List all the files in that directory */
    if ((sa = getFilenamesInDirectory(dir)) == NULL) {
        L_ERROR("directory %s does not exist!\n", __func__, dir);
        LEPT_FREE(dir);
        return 1;
    }
    nfiles = sarrayGetCount(sa);

    for (i = 0; i < nfiles; i++) {
        fname = sarrayGetString(sa, i, L_NOCOPY);
        fullname = genPathname(dir, fname);
        remove(fullname);
        LEPT_FREE(fullname);
    }

#ifndef _WIN32
    realdir = genPathname("/tmp", subdir);
    ret = rmdir(realdir);
    LEPT_FREE(realdir);
#else
    newpath = genPathname(dir, NULL);
    ret = (RemoveDirectoryA(newpath) ? 0 : 1);
    LEPT_FREE(newpath);
#endif  /* !_WIN32 */

    sarrayDestroy(&sa);
    LEPT_FREE(dir);
    return ret;
}


/*!
 * \brief   lept_direxists()
 *
 * \param[in]    dir
 * \param[out]   pexists    1 if it exists; 0 otherwise
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) Always use unix pathname separators.
 *      (2) By calling genPathname(), if the pathname begins with "/tmp"
 *          this does an automatic directory translation for operating
 *          systems that use a different path for /tmp.
 * </pre>
 */
void
lept_direxists(const char  *dir,
               l_int32     *pexists)
{
char  *realdir;

    if (!pexists) return;
    *pexists = 0;
    if (!dir) return;
    if ((realdir = genPathname(dir, NULL)) == NULL)
        return;

#ifndef _WIN32
    {
    struct stat s;
    l_int32 err = stat(realdir, &s);
    if (err != -1 && S_ISDIR(s.st_mode))
        *pexists = 1;
    }
#else  /* _WIN32 */
    {
    l_uint32  attributes;
    attributes = GetFileAttributesA(realdir);
    if (attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY))
        *pexists = 1;
    }
#endif  /* _WIN32 */

    LEPT_FREE(realdir);
}


/*!
 * \brief   lept_rm_match()
 *
 * \param[in]    subdir    [optional] if NULL, the removed files are in /tmp
 * \param[in]    substr    [optional] pattern to match in filename
 * \return  0 on success, non-zero on failure
 *
 * <pre>
 * Notes:
 *      (1) This removes the matched files in /tmp or a subdirectory of /tmp.
 *          Use NULL for %subdir if the files are in /tmp.
 *      (2) If %substr == NULL, this removes all files in the directory.
 *          If %substr == "" (empty), this removes no files.
 *          If both %subdir == NULL and %substr == NULL, this removes
 *          all files in /tmp.
 *      (3) Use unix pathname separators.
 *      (4) By calling genPathname(), if the pathname begins with "/tmp"
 *          this does an automatic directory translation for operating
 *          systems that use a different path for /tmp.
 *      (5) Error conditions:
 *            * returns -1 if the directory is not found
 *            * returns the number of files (> 0) that it was unable to remove.
 * </pre>
 */
l_int32
lept_rm_match(const char  *subdir,
              const char  *substr)
{
char    *path, *fname;
char     tempdir[256];
l_int32  i, n, ret;
SARRAY  *sa;

    makeTempDirname(tempdir, sizeof(tempdir), subdir);
    if ((sa = getSortedPathnamesInDirectory(tempdir, substr, 0, 0)) == NULL)
        return ERROR_INT("sa not made", __func__, -1);
    n = sarrayGetCount(sa);
    if (n == 0) {
        L_WARNING("no matching files found\n", __func__);
        sarrayDestroy(&sa);
        return 0;
    }

    ret = 0;
    for (i = 0; i < n; i++) {
        fname = sarrayGetString(sa, i, L_NOCOPY);
        path = genPathname(fname, NULL);
        if (lept_rmfile(path) != 0) {
            L_ERROR("failed to remove %s\n", __func__, path);
            ret++;
        }
        LEPT_FREE(path);
    }
    sarrayDestroy(&sa);
    return ret;
}


/*!
 * \brief   lept_rm()
 *
 * \param[in]    subdir    [optional] subdir of '/tmp'; can be NULL
 * \param[in]    tail      filename without the directory
 * \return  0 on success, non-zero on failure
 *
 * <pre>
 * Notes:
 *      (1) By calling genPathname(), this does an automatic directory
 *          translation on operating systems which use a different path.
 * </pre>
 */
l_int32
lept_rm(const char  *subdir,
        const char  *tail)
{
char    *path;
char     newtemp[256];
l_int32  ret;

    if (!tail || strlen(tail) == 0)
        return ERROR_INT("tail undefined or empty", __func__, 1);

    if (makeTempDirname(newtemp, sizeof(newtemp), subdir))
        return ERROR_INT("temp dirname not made", __func__, 1);
    path = genPathname(newtemp, tail);
    ret = lept_rmfile(path);
    LEPT_FREE(path);
    return ret;
}


/*!
 * \brief
 *
 *  lept_rmfile()
 *
 * \param[in]    filepath     full path to file including the directory
 * \return  0 on success, non-zero on failure
 *
 * <pre>
 * Notes:
 *      (1) This removes the named file.
 *      (2) Use unix pathname separators.
 *      (3) There is no name translation.
 *      (4) Unlike the other lept_* functions in this section, this can remove
 *          any file -- it is not restricted to files that are in /tmp or a
 *          subdirectory of it.
 *      (5) For files in /tmp or a subdirectory of it, this does an automatic
 *          directory translation for operating systems that use a different
 *          path for /tmp.
 * </pre>
 */
l_int32
lept_rmfile(const char  *filepath)
{
l_int32  ret;

    if (!filepath || strlen(filepath) == 0)
        return ERROR_INT("filepath undefined or empty", __func__, 1);

#ifndef _WIN32
    ret = remove(filepath);
#else
        /* Set attributes to allow deletion of read-only files */
    SetFileAttributesA(filepath, FILE_ATTRIBUTE_NORMAL);
    ret = DeleteFileA(filepath) ? 0 : 1;
#endif  /* !_WIN32 */

    return ret;
}


/*!
 * \brief   lept_mv()
 *
 * \param[in]    srcfile
 * \param[in]    newdir     [optional]; can be NULL
 * \param[in]    newtail    [optional]; can be NULL
 * \param[out]   pnewpath   [optional] of actual path; can be NULL
 * \return  0 on success, non-zero on failure
 *
 * <pre>
 * Notes:
 *      (1) This moves %srcfile to /tmp or to a subdirectory of /tmp.
 *      (2) %srcfile can either be a full path or relative to the
 *          current directory.
 *      (3) %newdir can either specify an existing subdirectory of /tmp
 *          or can be NULL.  In the latter case, the file will be written
 *          into /tmp.
 *      (4) %newtail can either specify a filename tail or, if NULL,
 *          the filename is taken from src-tail, the tail of %srcfile.
 *      (5) For debugging, the computed newpath can be returned.  It must
 *          be freed by the caller.
 *      (6) Reminders:
 *          (a) specify files using unix pathnames
 *          (b) this does an automatic directory translation on operating
 *              systems that use a different path for /tmp.
 *      (7) Examples:
 *          * newdir = NULL,    newtail = NULL    ==> /tmp/src-tail
 *          * newdir = NULL,    newtail = abc     ==> /tmp/abc
 *          * newdir = def/ghi, newtail = NULL    ==> /tmp/def/ghi/src-tail
 *          * newdir = def/ghi, newtail = abc     ==> /tmp/def/ghi/abc
 * </pre>
 */
l_int32
lept_mv(const char  *srcfile,
        const char  *newdir,
        const char  *newtail,
        char       **pnewpath)
{
char    *srcpath, *newpath, *dir, *srctail;
char     newtemp[256];
l_int32  ret;

    if (!srcfile)
        return ERROR_INT("srcfile not defined", __func__, 1);

        /* Require output pathname to be in /tmp/ or a subdirectory */
    if (makeTempDirname(newtemp, sizeof(newtemp), newdir) == 1)
        return ERROR_INT("newdir not NULL or a subdir of /tmp", __func__, 1);

        /* Get canonical src pathname */
    splitPathAtDirectory(srcfile, &dir, &srctail);

#ifndef _WIN32
    srcpath = pathJoin(dir, srctail);
    LEPT_FREE(dir);

        /* Generate output pathname */
    if (!newtail || newtail[0] == '\0')
        newpath = pathJoin(newtemp, srctail);
    else
        newpath = pathJoin(newtemp, newtail);
    LEPT_FREE(srctail);

        /* Overwrite any existing file at 'newpath' */
    ret = fileCopy(srcpath, newpath);
    if (!ret) {  /* and remove srcfile */
        char *realpath = genPathname(srcpath, NULL);
        remove(realpath);
        LEPT_FREE(realpath);
    }
#else
    srcpath = genPathname(dir, srctail);
    LEPT_FREE(dir);

        /* Generate output pathname */
    if (!newtail || newtail[0] == '\0')
        newpath = genPathname(newtemp, srctail);
    else
        newpath = genPathname(newtemp, newtail);
    LEPT_FREE(srctail);

        /* Overwrite any existing file at 'newpath' */
    ret = MoveFileExA(srcpath, newpath,
                     MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING) ? 0 : 1;
#endif  /* ! _WIN32 */

    LEPT_FREE(srcpath);
    if (pnewpath)
        *pnewpath = newpath;
    else
        LEPT_FREE(newpath);
    return ret;
}


/*!
 * \brief   lept_cp()
 *
 * \param[in]    srcfile
 * \param[in]    newdir    [optional]; can be NULL
 * \param[in]    newtail   [optional]; can be NULL
 * \param[out]   pnewpath  [optional] of actual path; can be NULL
 * \return  0 on success, non-zero on failure
 *
 * <pre>
 * Notes:
 *      (1) This copies %srcfile to /tmp or to a subdirectory of /tmp.
 *      (2) %srcfile can either be a full path or relative to the
 *          current directory.
 *      (3) %newdir can either specify an existing subdirectory of /tmp,
 *          or can be NULL.  In the latter case, the file will be written
 *          into /tmp.
 *      (4) %newtail can either specify a filename tail or, if NULL,
 *          the filename is taken from src-tail, the tail of %srcfile.
 *      (5) For debugging, the computed newpath can be returned.  It must
 *          be freed by the caller.
 *      (6) Reminders:
 *          (a) specify files using unix pathnames
 *          (b) this does an automatic directory translation for operating
 *              systems that use a different path for /tmp
 *      (7) Examples:
 *          * newdir = NULL,    newtail = NULL    ==> /tmp/src-tail
 *          * newdir = NULL,    newtail = abc     ==> /tmp/abc
 *          * newdir = def/ghi, newtail = NULL    ==> /tmp/def/ghi/src-tail
 *          * newdir = def/ghi, newtail = abc     ==> /tmp/def/ghi/abc
 *
 * </pre>
 */
l_int32
lept_cp(const char  *srcfile,
        const char  *newdir,
        const char  *newtail,
        char       **pnewpath)
{
char    *srcpath, *newpath, *dir, *srctail;
char     newtemp[256];
l_int32  ret;

    if (!srcfile)
        return ERROR_INT("srcfile not defined", __func__, 1);

        /* Require output pathname to be in /tmp or a subdirectory */
    if (makeTempDirname(newtemp, sizeof(newtemp), newdir) == 1)
        return ERROR_INT("newdir not NULL or a subdir of /tmp", __func__, 1);

       /* Get canonical src pathname */
    splitPathAtDirectory(srcfile, &dir, &srctail);

#ifndef _WIN32
    srcpath = pathJoin(dir, srctail);
    LEPT_FREE(dir);

        /* Generate output pathname */
    if (!newtail || newtail[0] == '\0')
        newpath = pathJoin(newtemp, srctail);
    else
        newpath = pathJoin(newtemp, newtail);
    LEPT_FREE(srctail);

        /* Overwrite any existing file at 'newpath' */
    ret = fileCopy(srcpath, newpath);
#else
    srcpath = genPathname(dir, srctail);
    LEPT_FREE(dir);

        /* Generate output pathname */
    if (!newtail || newtail[0] == '\0')
        newpath = genPathname(newtemp, srctail);
    else
        newpath = genPathname(newtemp, newtail);
    LEPT_FREE(srctail);

        /* Overwrite any existing file at 'newpath' */
    ret = CopyFileA(srcpath, newpath, FALSE) ? 0 : 1;
#endif   /* !_WIN32 */

    LEPT_FREE(srcpath);
    if (pnewpath)
        *pnewpath = newpath;
    else
        LEPT_FREE(newpath);
    return ret;
}


/*--------------------------------------------------------------------*
 *          Special debug/test function for calling 'system'          *
 *--------------------------------------------------------------------*/
#if defined(__APPLE__)
  #include "TargetConditionals.h"
#endif  /* __APPLE__ */

/*!
 * \brief   callSystemDebug()
 *
 * \param[in]    cmd      command to be exec'd
 * \return  0 on success
 *
 * <pre>
 * Notes:
 *      (1) The C library 'system' call is only made through this function.
 *          It only works in debug/test mode, where the global variable
 *          LeptDebugOK == TRUE.  This variable is set to FALSE in the
 *          library as distributed, and calling this function will
 *          generate an error message.
 * </pre>
 */
l_int32
callSystemDebug(const char *cmd)
{
l_int32  ret;

    if (!cmd) {
        L_ERROR("cmd not defined\n", __func__);
        return 1;
    }
    if (LeptDebugOK == FALSE) {
        L_INFO("'system' calls are disabled\n", __func__);
        return 1;
    }

#if defined(__APPLE__)  /* iOS 11 does not support system() */

  #if (defined(TARGET_OS_OSX) && TARGET_OS_OSX == 1)  /* Mac OS X */
    ret = system(cmd);
  #elif TARGET_OS_IPHONE || defined(OS_IOS)  /* iOS */
    L_ERROR("iOS 11 does not support system()\n", __func__);
  #endif  /* TARGET_OS_OSX */

#else /* ! __APPLE__ */

   ret = system(cmd);

#endif /* __APPLE__ */

   return ret;
}


/*--------------------------------------------------------------------*
 *                     General file name operations                   *
 *--------------------------------------------------------------------*/
/*!
 * \brief   splitPathAtDirectory()
 *
 * \param[in]    pathname  full path; can be a directory
 * \param[out]   pdir      [optional] root directory name of
 *                         input path, including trailing '/'
 * \param[out]   ptail     [optional] path tail, which is either
 *                         the file name within the root directory or
 *                         the last sub-directory in the path
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If you only want the tail, input null for the root directory ptr.
 *      (2) If you only want the root directory name, input null for the
 *          tail ptr.
 *      (3) This function makes decisions based only on the lexical
 *          structure of the input.  Examples:
 *            /usr/tmp/abc.d  -->  dir: /usr/tmp/       tail: abc.d
 *            /usr/tmp/       -->  dir: /usr/tmp/       tail: [empty string]
 *            /usr/tmp        -->  dir: /usr/           tail: tmp
 *            abc.d           -->  dir: [empty string]  tail: abc.d
 *      (4  Consider the first example above: /usr/tmp/abc.d.
 *          Suppose you want the stem of the file, abc, without either
 *          the directory or the extension.  This can be extracted in two steps:
 *              splitPathAtDirectory("usr/tmp/abc.d", NULL, &tail);
 *                   [sets tail: "abc.d"]
 *              splitPathAtExtension(tail, &basename, NULL);
 *                   [sets basename: "abc"]
 *      (5) The input can have either forward (unix) or backward (win)
 *          slash separators.  The output has unix separators.
 *          Note that Win32 pathname functions generally accept both
 *          slash forms, but the Windows command line interpreter
 *          only accepts backward slashes, because forward slashes are
 *          used to demarcate switches (vs. dashes in unix).
 * </pre>
 */
l_ok
splitPathAtDirectory(const char  *pathname,
                     char       **pdir,
                     char       **ptail)
{
char  *cpathname, *lastslash;

    if (!pdir && !ptail)
        return ERROR_INT("null input for both strings", __func__, 1);
    if (pdir) *pdir = NULL;
    if (ptail) *ptail = NULL;
    if (!pathname)
        return ERROR_INT("pathname not defined", __func__, 1);

    cpathname = stringNew(pathname);
    convertSepCharsInPath(cpathname, UNIX_PATH_SEPCHAR);
    lastslash = strrchr(cpathname, '/');
    if (lastslash) {
        if (ptail)
            *ptail = stringNew(lastslash + 1);
        if (pdir) {
            *(lastslash + 1) = '\0';
            *pdir = cpathname;
        } else {
            LEPT_FREE(cpathname);
        }
    } else {  /* no directory */
        if (pdir)
            *pdir = stringNew("");
        if (ptail)
            *ptail = cpathname;
        else
            LEPT_FREE(cpathname);
    }

    return 0;
}


/*!
 * \brief   splitPathAtExtension()
 *
 * \param[in]    pathname    full path; can be a directory
 * \param[out]   pbasename   [optional] pathname not including the
 *                           last dot and characters after that
 * \param[out]   pextension  [optional] path extension, which is
 *                           the last dot and the characters after it.  If
 *                           there is no extension, it returns the empty string
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If you only want the extension, input null for the basename ptr.
 *      (2) If you only want the basename without extension, input null
 *          for the extension ptr.
 *      (3) This function makes decisions based only on the lexical
 *          structure of the input.  Examples:
 *            /usr/tmp/abc.jpg  -->  basename: /usr/tmp/abc    ext: .jpg
 *            /usr/tmp/.jpg     -->  basename: /usr/tmp/       ext: .jpg
 *            /usr/tmp.jpg/     -->  basename: /usr/tmp.jpg/   ext: [empty str]
 *            ./.jpg            -->  basename: ./              ext: .jpg
 *      (4) The input can have either forward (unix) or backward (win)
 *          slash separators.  The output has unix separators.
 *      (5) Note that basename, as used here, is different from the result
 *          of the unix program 'basename'.  Here, basename is the entire
 *          pathname up to a final extension and its preceding dot.
 * </pre>
 */
l_ok
splitPathAtExtension(const char  *pathname,
                     char       **pbasename,
                     char       **pextension)
{
char  *tail, *dir, *lastdot;
char   empty[4] = "";

    if (!pbasename && !pextension)
        return ERROR_INT("null input for both strings", __func__, 1);
    if (pbasename) *pbasename = NULL;
    if (pextension) *pextension = NULL;
    if (!pathname)
        return ERROR_INT("pathname not defined", __func__, 1);

        /* Split out the directory first */
    splitPathAtDirectory(pathname, &dir, &tail);

        /* Then look for a "." in the tail part.
         * This way we ignore all "." in the directory. */
    if ((lastdot = strrchr(tail, '.'))) {
        if (pextension)
            *pextension = stringNew(lastdot);
        if (pbasename) {
            *lastdot = '\0';
            *pbasename = stringJoin(dir, tail);
        }
    } else {
        if (pextension)
            *pextension = stringNew(empty);
        if (pbasename)
            *pbasename = stringNew(pathname);
    }
    LEPT_FREE(dir);
    LEPT_FREE(tail);
    return 0;
}


/*!
 * \brief   pathJoin()
 *
 * \param[in]    dir     [optional] can be null
 * \param[in]    fname   [optional] can be null
 * \return  specially concatenated path, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Use unix-style pathname separators ('/').
 *      (2) %fname can be the entire path, or part of the path containing
 *          at least one directory, or a tail without a directory, or NULL.
 *      (3) It produces a path that strips multiple slashes to a single
 *          slash, joins %dir and %fname by a slash, and has no trailing
 *          slashes (except in the cases where %dir == "/" and
 *          %fname == NULL, or v.v.).
 *      (4) If both %dir and %fname are null, produces an empty string.
 *      (5) Neither %dir nor %fname can begin with '..'.
 *      (6) The result is not canonicalized or tested for correctness:
 *          garbage in (e.g., /&%), garbage out.
 *      (7) Examples:
 *             //tmp// + //abc/  -->  /tmp/abc
 *             tmp/ + /abc/      -->  tmp/abc
 *             tmp/ + abc/       -->  tmp/abc
 *             /tmp/ + ///       -->  /tmp
 *             /tmp/ + NULL      -->  /tmp
 *             // + /abc//       -->  /abc
 *             // + NULL         -->  /
 *             NULL + /abc/def/  -->  /abc/def
 *             NULL + abc//      -->  abc
 *             NULL + //         -->  /
 *             NULL + NULL       -->  (empty string)
 *             "" + ""           -->  (empty string)
 *             "" + /            -->  /
 *             ".." + /etc/foo   -->  NULL
 *             /tmp + ".."       -->  NULL
 * </pre>
 */
char *
pathJoin(const char  *dir,
         const char  *fname)
{
const char *slash = "/";
char       *str, *dest;
l_int32     i, n1, n2, emptydir;
size_t      size;
SARRAY     *sa1, *sa2;
L_BYTEA    *ba;

    if (!dir && !fname)
        return stringNew("");
    if (dir && strlen(dir) >= 2 && dir[0] == '.' && dir[1] == '.')
        return (char *)ERROR_PTR("dir starts with '..'", __func__, NULL);
    if (fname && strlen(fname) >= 2 && fname[0] == '.' && fname[1] == '.')
        return (char *)ERROR_PTR("fname starts with '..'", __func__, NULL);

    sa1 = sarrayCreate(0);
    sa2 = sarrayCreate(0);
    ba = l_byteaCreate(4);

        /* Process %dir */
    if (dir && strlen(dir) > 0) {
        if (dir[0] == '/')
            l_byteaAppendString(ba, slash);
        sarraySplitString(sa1, dir, "/");  /* removes all slashes */
        n1 = sarrayGetCount(sa1);
        for (i = 0; i < n1; i++) {
            str = sarrayGetString(sa1, i, L_NOCOPY);
            l_byteaAppendString(ba, str);
            l_byteaAppendString(ba, slash);
        }
    }

        /* Special case to add leading slash: dir NULL or empty string  */
    emptydir = dir && strlen(dir) == 0;
    if ((!dir || emptydir) && fname && strlen(fname) > 0 && fname[0] == '/')
        l_byteaAppendString(ba, slash);

        /* Process %fname */
    if (fname && strlen(fname) > 0) {
        sarraySplitString(sa2, fname, "/");
        n2 = sarrayGetCount(sa2);
        for (i = 0; i < n2; i++) {
            str = sarrayGetString(sa2, i, L_NOCOPY);
            l_byteaAppendString(ba, str);
            l_byteaAppendString(ba, slash);
        }
    }

        /* Remove trailing slash */
    dest = (char *)l_byteaCopyData(ba, &size);
    if (size > 1 && dest[size - 1] == '/')
        dest[size - 1] = '\0';

    sarrayDestroy(&sa1);
    sarrayDestroy(&sa2);
    l_byteaDestroy(&ba);
    return dest;
}


/*!
 * \brief   appendSubdirs()
 *
 * \param[in]    basedir
 * \param[in]    subdirs
 * \return  concatenated full directory path without trailing slash,
 *              or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Use unix pathname separators
 *      (2) Allocates a new string:  [basedir]/[subdirs]
 * </pre>
 */
char *
appendSubdirs(const char  *basedir,
              const char  *subdirs)
{
char   *newdir;
size_t  len1, len2, len3, len4;

    if (!basedir || !subdirs)
        return (char *)ERROR_PTR("basedir and subdirs not both defined",
                                 __func__, NULL);

    len1 = strlen(basedir);
    len2 = strlen(subdirs);
    len3 = len1 + len2 + 8;
    if ((newdir = (char *)LEPT_CALLOC(len3, 1)) == NULL)
        return (char *)ERROR_PTR("newdir not made", __func__, NULL);
    stringCat(newdir, len3, basedir);
    if (newdir[len1 - 1] != '/')  /* add '/' if necessary */
        newdir[len1] = '/';
    if (subdirs[0] == '/')  /* add subdirs, stripping leading '/' */
        stringCat(newdir, len3, subdirs + 1);
    else
        stringCat(newdir, len3, subdirs);
    len4 = strlen(newdir);
    if (newdir[len4 - 1] == '/')  /* strip trailing '/' */
        newdir[len4 - 1] = '\0';

    return newdir;
}


/*--------------------------------------------------------------------*
 *                     Special file name operations                   *
 *--------------------------------------------------------------------*/
/*!
 * \brief   convertSepCharsInPath()
 *
 * \param[in]    path
 * \param[in]    type    UNIX_PATH_SEPCHAR, WIN_PATH_SEPCHAR
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) In-place conversion.
 *      (2) Type is the resulting type:
 *            * UNIX_PATH_SEPCHAR:  '\\' ==> '/'
 *            * WIN_PATH_SEPCHAR:   '/' ==> '\\'
 *      (3) Virtually all path operations in leptonica use unix separators.
 *      (4) The backslash is a valid character in unix pathnames and should
 *          not be converted.  Each backslash needs to be escaped with a
 *          preceding backslash for the shell, but the actual filename
 *          does not include these escape characters.
 * </pre>
 */
l_ok
convertSepCharsInPath(char    *path,
                      l_int32  type)
{
l_int32  i;
size_t   len;

    if (!path)
        return ERROR_INT("path not defined", __func__, 1);
    if (type != UNIX_PATH_SEPCHAR && type != WIN_PATH_SEPCHAR)
        return ERROR_INT("invalid type", __func__, 1);

    len = strlen(path);
    if (type == UNIX_PATH_SEPCHAR) {
#ifdef _WIN32  /* only convert on Windows */
        for (i = 0; i < len; i++) {
            if (path[i] == '\\')
                path[i] = '/';
        }
#endif  /* _WIN32 */
    } else {  /* WIN_PATH_SEPCHAR */
        for (i = 0; i < len; i++) {
            if (path[i] == '/')
                path[i] = '\\';
        }
    }
    return 0;
}


/*!
 * \brief   genPathname()
 *
 * \param[in]    dir     [optional] directory or full path name,
 *                       with or without the trailing '/'
 * \param[in]    fname   [optional] file name within a directory
 * \return  pathname either a directory or full path, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This function generates actual paths in the following ways:
 *            * from two sub-parts (e.g., a directory and a file name).
 *            * from a single path full path, placed in %dir, with
 *              %fname == NULL.
 *            * from the name of a file in the local directory placed in
 *              %fname, with %dir == NULL.
 *            * if in a "/tmp" directory and on iOS, macOS or Windows,
 *              the OS specific temp directory is used.
 *      (2) This does an automatic directory translation for operating
 *          systems that use a different path for /tmp.
 *          That path is determined
 *             * on Windows: by GetTempPath()
 *             * on macOS, iOS: by confstr() (see man page)
 *      (3) On unix, the TMPDIR variable is ignored.  No rewriting
 *          of temp directories is permitted.
 *      (4) There are four cases for the input:
 *          (a) %dir is a directory and %fname is defined: result is a
 *              full path
 *          (b) %dir is a directory and %fname is null: result is a directory
 *          (c) %dir is a full path and %fname is null: result is a full path
 *          (d) %dir is null or an empty string: start in the current dir;
 *              result is a full path
 *      (5) In all cases, the resulting pathname is not terminated with a slash
 *      (6) The caller is responsible for freeing the returned pathname.
 * </pre>
 */
char *
genPathname(const char  *dir,
            const char  *fname)
{
#if defined(REWRITE_TMP)
l_int32  rewrite_tmp = TRUE;
#else
l_int32  rewrite_tmp = FALSE;
#endif  /* REWRITE_TMP */
char    *cdir, *pathout;
l_int32  dirlen, namelen;
size_t   size;

    if (!dir && !fname)
        return (char *)ERROR_PTR("no input", __func__, NULL);

        /* Handle the case where we start from the current directory */
    if (!dir || dir[0] == '\0') {
        if ((cdir = getcwd(NULL, 0)) == NULL)
            return (char *)ERROR_PTR("no current dir found", __func__, NULL);
    } else {
        if ((cdir = stringNew(dir)) == NULL)
            return (char *)ERROR_PTR("stringNew failed", __func__, NULL);
    }

        /* Convert to unix path separators, and remove the trailing
         * slash in the directory, except when dir == "/"  */
    convertSepCharsInPath(cdir, UNIX_PATH_SEPCHAR);
    dirlen = strlen(cdir);
    if (cdir[dirlen - 1] == '/' && dirlen != 1) {
        cdir[dirlen - 1] = '\0';
        dirlen--;
    }

    namelen = (fname) ? strlen(fname) : 0;
    size = dirlen + namelen + 256;
    if ((pathout = (char *)LEPT_CALLOC(size, sizeof(char))) == NULL) {
        LEPT_FREE(cdir);
        return (char *)ERROR_PTR("pathout not made", __func__, NULL);
    }

        /* First handle %dir (which may be a full pathname).
         * There is no path rewriting on unix, and on win32, we do not
         * rewrite unless the specified directory is /tmp or
         * a subdirectory of /tmp */
    if (!rewrite_tmp || dirlen < 4 ||
        (dirlen == 4 && strncmp(cdir, "/tmp", 4) != 0) ||  /* not in "/tmp" */
        (dirlen > 4 && strncmp(cdir, "/tmp/", 5) != 0)) {  /* not in "/tmp/" */
        stringCopy(pathout, cdir, dirlen);
    } else {  /* Rewrite with "/tmp" specified for the directory. */
#if defined(__APPLE__)
        size_t n = confstr(_CS_DARWIN_USER_TEMP_DIR, pathout, size);
        if (n == 0 || n > size) {
            /* Fall back to using /tmp */
            stringCopy(pathout, cdir, dirlen);
        } else {
            /* Add the rest of cdir */
            if (dirlen > 4)
                stringCat(pathout, size, cdir + 4);
        }
#elif defined(_WIN32)
        l_int32 tmpdirlen;
        char tmpdir[MAX_PATH];
        GetTempPathA(sizeof(tmpdir), tmpdir);  /* get the Windows temp dir */
        tmpdirlen = strlen(tmpdir);
        if (tmpdirlen > 0 && tmpdir[tmpdirlen - 1] == '\\') {
            tmpdir[tmpdirlen - 1] = '\0';  /* trim the trailing '\' */
        }
        tmpdirlen = strlen(tmpdir);
        stringCopy(pathout, tmpdir, tmpdirlen);

            /* Add the rest of cdir */
        if (dirlen > 4)
            stringCat(pathout, size, cdir + 4);
#endif  /* _WIN32 */
    }

        /* Now handle %fname */
    if (fname && strlen(fname) > 0) {
        dirlen = strlen(pathout);
        pathout[dirlen] = '/';
        stringCat(pathout, size, fname);
    }

    LEPT_FREE(cdir);
    return pathout;
}


/*!
 * \brief   makeTempDirname()
 *
 * \param[in]    result    preallocated on stack or heap and passed in
 * \param[in]    nbytes    size of %result array, in bytes
 * \param[in]    subdir    [optional]; can be NULL or an empty string
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This generates the directory path for output temp files,
 *          written into %result with unix separators.
 *      (2) Caller allocates %result, large enough to hold the path,
 *          which is:
 *            /tmp/%subdir       (unix)
 *            [Temp]/%subdir     (Windows, macOS, iOS)
 *          where [Temp] is the OS path
 *          and %subdir is in general a set of nested subdirectories:
 *            dir1/dir2/.../dirN
 *          which in use would not typically exceed 2 levels.
 *      (3) Usage example:
 * \code
 *           char  result[256];
 *           makeTempDirname(result, sizeof(result), "lept/golden");
 * \endcode
 * </pre>
 */
l_ok
makeTempDirname(char        *result,
                size_t       nbytes,
                const char  *subdir)
{
char    *dir, *path;
l_int32  ret = 0;
size_t   pathlen;

    if (!result)
        return ERROR_INT("result not defined", __func__, 1);
    if (subdir && ((subdir[0] == '.') || (subdir[0] == '/')))
        return ERROR_INT("subdir not an actual subdirectory", __func__, 1);

    memset(result, 0, nbytes);

    dir = pathJoin("/tmp", subdir);

#if defined(REWRITE_TMP)
    path = genPathname(dir, NULL);
#else
    path = stringNew(dir);
#endif  /*  ~ _WIN32 */
    pathlen = strlen(path);
    if (pathlen < nbytes - 1) {
        stringCopy(result, path, nbytes);
    } else {
        L_ERROR("result array too small for path\n", __func__);
        ret = 1;
    }

    LEPT_FREE(dir);
    LEPT_FREE(path);
    return ret;
}


/*!
 * \brief   modifyTrailingSlash()
 *
 * \param[in]    path     preallocated on stack or heap and passed in
 * \param[in]    nbytes   size of %path array, in bytes
 * \param[in]    flag     L_ADD_TRAIL_SLASH or L_REMOVE_TRAIL_SLASH
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This carries out the requested action if necessary.
 * </pre>
 */
l_ok
modifyTrailingSlash(char    *path,
                    size_t   nbytes,
                    l_int32  flag)
{
char    lastchar;
size_t  len;

    if (!path)
        return ERROR_INT("path not defined", __func__, 1);
    if (flag != L_ADD_TRAIL_SLASH && flag != L_REMOVE_TRAIL_SLASH)
        return ERROR_INT("invalid flag", __func__, 1);

    len = strlen(path);
    lastchar = path[len - 1];
    if (flag == L_ADD_TRAIL_SLASH && lastchar != '/' && len < nbytes - 2) {
        path[len] = '/';
        path[len + 1] = '\0';
    } else if (flag == L_REMOVE_TRAIL_SLASH && lastchar == '/') {
        path[len - 1] = '\0';
    }
    return 0;
}


/*!
 * \brief   l_makeTempFilename()
 *
 * \return  fname : heap allocated filename; returns NULL on failure.
 *
 * <pre>
 * Notes:
 *      (1) On unix, this makes a filename of the form
 *               "/tmp/lept.XXXXXX",
 *          where each X is a random character.
 *      (2) On Windows, this makes a filename of the form
 *               "/[Temp]/lp.XXXXXX".
 *      (3) On all systems, this fails if the file is not writable.
 *      (4) Safest usage is to write to a subdirectory in debug code.
 *      (5) The returned filename must be freed by the caller, using lept_free.
 *      (6) The tail of the filename has a '.', so that cygwin interprets
 *          the file as having an extension.  Otherwise, cygwin assumes it
 *          is an executable and appends ".exe" to the filename.
 *      (7) On unix, whenever possible use tmpfile() instead.  tmpfile()
 *          hides the file name, returns a stream opened for write,
 *          and deletes the temp file when the stream is closed.
 * </pre>
 */
char *
l_makeTempFilename(void)
{
char  dirname[240];

    if (makeTempDirname(dirname, sizeof(dirname), NULL) == 1)
        return (char *)ERROR_PTR("failed to make dirname", __func__, NULL);

#ifndef _WIN32
{
    char    *pattern;
    l_int32  fd;
    pattern = stringConcatNew(dirname, "/lept.XXXXXX", NULL);
    fd = mkstemp(pattern);
    if (fd == -1) {
        LEPT_FREE(pattern);
        return (char *)ERROR_PTR("mkstemp failed", __func__, NULL);
    }
    close(fd);
    return pattern;
}
#else
{
    char  fname[MAX_PATH];
    FILE *fp;
    if (GetTempFileNameA(dirname, "lp.", 0, fname) == 0)
        return (char *)ERROR_PTR("GetTempFileName failed", __func__, NULL);
    if ((fp = fopen(fname, "wb")) == NULL)
        return (char *)ERROR_PTR("file cannot be written to", __func__, NULL);
    fclose(fp);
    return stringNew(fname);
}
#endif  /*  ~ _WIN32 */
}


/*!
 * \brief   extractNumberFromFilename()
 *
 * \param[in]    fname
 * \param[in]    numpre    number of characters before the digits to be found
 * \param[in]    numpost   number of characters after the digits to be found
 * \return  num number embedded in the filename; -1 on error or if
 *                   not found
 *
 * <pre>
 * Notes:
 *      (1) The number is to be found in the basename, which is the
 *          filename without either the directory or the last extension.
 *      (2) When a number is found, it is non-negative.  If no number
 *          is found, this returns -1, without an error message.  The
 *          caller needs to check.
 * </pre>
 */
l_int32
extractNumberFromFilename(const char  *fname,
                          l_int32      numpre,
                          l_int32      numpost)
{
char    *tail, *basename;
l_int32  len, nret, num;

    if (!fname)
        return ERROR_INT("fname not defined", __func__, -1);

    splitPathAtDirectory(fname, NULL, &tail);
    splitPathAtExtension(tail, &basename, NULL);
    LEPT_FREE(tail);

    len = strlen(basename);
    if (numpre + numpost > len - 1) {
        LEPT_FREE(basename);
        return ERROR_INT("numpre + numpost too big", __func__, -1);
    }

    basename[len - numpost] = '\0';
    nret = sscanf(basename + numpre, "%d", &num);
    LEPT_FREE(basename);

    if (nret == 1)
        return num;
    else
        return -1;  /* not found */
}
