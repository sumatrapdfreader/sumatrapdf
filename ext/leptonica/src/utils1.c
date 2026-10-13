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
 * \file utils1.c
 * <pre>
 *
 *       ------------------------------------------
 *       This file has these utilities:
 *         - error, warning and info messages
 *         - redirection of stderr
 *         - low-level endian conversions
 *         - file corruption operations
 *         - random and prime number operations
 *         - 64-bit hash functions
 *         - leptonica version number accessor
 *         - timing and date operations
 *       ------------------------------------------
 *
 *       Control of error, warning and info messages
 *           l_int32    setMsgSeverity()
 *
 *       Error return functions, invoked by macros
 *           l_int32    returnErrorInt()
 *           l_float32  returnErrorFloat()
 *           void      *returnErrorPtr()
 *           l_int32    returnErrorInt1()
 *           l_float32  returnErrorFloat1()
 *           void      *returnErrorPtr1()
 *
 *       Runtime redirection of stderr
 *           void leptSetStderrHandler()
 *           void lept_stderr()
 *
 *       Test files for equivalence
 *           l_int32    filesAreIdentical()
 *
 *       Byte-swapping data conversion
 *           l_uint16   convertOnBigEnd16()
 *           l_uint32   convertOnBigEnd32()
 *           l_uint16   convertOnLittleEnd16()
 *           l_uint32   convertOnLittleEnd32()
 *
 *       File corruption and byte replacement operations
 *           l_int32    fileCorruptByDeletion()
 *           l_int32    fileCorruptByMutation()
 *           l_int32    fileReplaceBytes()
 *
 *       Generate random integer in given interval
 *           l_int32    genRandomIntOnInterval()
 *
 *       Simple math functions
 *           l_int32    lept_roundftoi()
 *           l_int32    lept_floor()
 *           l_int32    lept_ceiling()
 *
 *       64-bit hash functions
 *           l_int32    l_hashStringToUint64()
 *           l_int32    l_hashStringToUint64Fast()
 *           l_int32    l_hashPtToUint64()
 *           l_int32    l_hashFloat64ToUint64()
 *
 *       Prime finders
 *           l_int32    findNextLargerPrime()
 *           l_int32    lept_isPrime()
 *
 *       Gray code conversion
 *           l_uint32   convertIntToGrayCode()
 *           l_uint32   convertGrayCodeToInt()
 *
 *       Leptonica version number
 *           char      *getLeptonicaVersion()
 *
 *       Timing
 *           void       startTimer()
 *           l_float32  stopTimer()
 *           L_TIMER    startTimerNested()
 *           l_float32  stopTimerNested()
 *           void       l_getCurrentTime()
 *           L_WALLTIMER  *startWallTimer()
 *           l_float32  stopWallTimer()
 *           void       l_getFormattedDate()
 *
 *  For all issues with cross-platform development, see utils2.c.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#ifdef _WIN32
#include <windows.h>
#endif  /* _WIN32 */

#include <time.h>
#include "allheaders.h"
#include <math.h>

    /* Global for controlling message output at runtime */
LEPT_DLL l_int32  LeptMsgSeverity = DEFAULT_SEVERITY;

#define  DEBUG_SEV     0

/*----------------------------------------------------------------------*
 *                Control of error, warning and info messages           *
 *----------------------------------------------------------------------*/
/*!
 * \brief   setMsgSeverity()
 *
 * \param[in]    newsev
 * \return  oldsev
 *
 * <pre>
 * Notes:
 *      (1) setMsgSeverity() allows the user to specify the desired
 *          message severity threshold.  Messages of equal or greater
 *          severity will be output.  The previous message severity is
 *          returned when the new severity is set.
 *      (2) If L_SEVERITY_EXTERNAL is passed, then the severity will be
 *          obtained from the LEPT_MSG_SEVERITY environment variable.
 * </pre>
 */
l_int32
setMsgSeverity(l_int32  newsev)
{
l_int32  oldsev;
char    *envsev;

    oldsev = LeptMsgSeverity;
    if (newsev == L_SEVERITY_EXTERNAL) {
        envsev = getenv("LEPT_MSG_SEVERITY");
        if (envsev) {
            LeptMsgSeverity = atoi(envsev);
#if DEBUG_SEV
            L_INFO("message severity set to external\n", "setMsgSeverity");
#endif  /* DEBUG_SEV */
        } else {
#if DEBUG_SEV
            L_WARNING("environment var LEPT_MSG_SEVERITY not defined\n",
                      "setMsgSeverity");
#endif  /* DEBUG_SEV */
        }
    } else {
        LeptMsgSeverity = newsev;
#if DEBUG_SEV
        L_INFO("message severity set to %d\n", "setMsgSeverity", newsev);
#endif  /* DEBUG_SEV */
    }

    return oldsev;
}


/*----------------------------------------------------------------------*
 *                Error return functions, invoked by macros             *
 *----------------------------------------------------------------------*
 *                                                                      *
 *    (1) These error functions print messages to stderr and allow      *
 *        exit from the function that called them.                      *
 *    (2) They must be invoked only by the three argument macros        *
 *           ERROR_INT, ERROR_FLOAT, ERROR_PTR                          *
 *        or the four argument macros                                   *
 *           ERROR_INT_1, ERROR_FLOAT_1, ERROR_PTR_1                    *
 *        which are in environ.h.                                       *
 *    (3) The print output can be disabled at compile time, either      *
 *        by using -DNO_CONSOLE_IO or by setting LeptMsgSeverity.       *
 *----------------------------------------------------------------------*/
/*!
 * \brief   returnErrorInt()
 *
 * \param[in]    msg        error message
 * \param[in]    procname   use __func__
 * \param[in]    ival       return error val (typically 1 for an error)  
 * \return  ival
 */
l_int32
returnErrorInt(const char  *msg,
               const char  *procname,
               l_int32      ival)
{
    lept_stderr("Error in %s: %s\n", procname, msg);
    return ival;
}


/*!
 * \brief   returnErrorFloat()
 *
 * \param[in]    msg        error message
 * \param[in]    procname   use __func__
 * \param[in]    fval       return float error val
 * \return  fval
 */
l_float32
returnErrorFloat(const char  *msg,
                 const char  *procname,
                 l_float32    fval)
{
    lept_stderr("Error in %s: %s\n", procname, msg);
    return fval;
}


/*!
 * \brief   returnErrorPtr()
 *
 * \param[in]    msg        error message
 * \param[in]    procname   use __func__
 * \param[in]    pval       return error val (typically null for an error)
 * \return  pval
 */
void *
returnErrorPtr(const char  *msg,
               const char  *procname,
               void        *pval)
{
    lept_stderr("Error in %s: %s\n", procname, msg);
    return pval;
}


/*!
 * \brief   returnErrorInt1()
 *
 * \param[in]    msg        error message
 * \param[in]    arg        additional error message argument
 *                          (will be appended to the error message)
 * \param[in]    procname   use __func__
 * \param[in]    ival       return error val; typically 1 for an error return
 * \return  ival   typically 1 for an error return
 */
l_int32
returnErrorInt1(const char  *msg,
                const char  *arg,
                const char  *procname,
                l_int32      ival)
{
    lept_stderr("Leptonica Error in %s: %s: %s\n", procname, msg, arg);
    return ival;
}


/*!
 * \brief   returnErrorFloat1()
 *
 * \param[in]    msg        error message
 * \param[in]    arg        additional error message argument
 *                          (will be appended to the error message)
 * \param[in]    procname   use __func__
 * \param[in]    fval       return float error val
 * \return  fval
 */
l_float32
returnErrorFloat1(const char  *msg,
                  const char  *arg,
                  const char  *procname,
                  l_float32    fval)
{
    lept_stderr("Leptonica Error in %s: %s: %s\n", procname, msg, arg);
    return fval;
}


/*!
 * \brief   returnErrorPtr1()
 *
 * \param[in]    msg        error message
 * \param[in]    arg        additional error message argument
 *                          (will be appended to the error message)
 * \param[in]    procname   use __func__
 * \param[in]    pval       return error val (typically null for an error)
 * \return  pval
 */
void *
returnErrorPtr1(const char  *msg,
                const char  *arg,
                const char  *procname,
                void        *pval)
{
    lept_stderr("Leptonica Error in %s: %s: %s\n", procname, msg, arg);
    return pval;
}


/*------------------------------------------------------------------------*
 *                   Runtime redirection of stderr                        *
 *------------------------------------------------------------------------*
 *                                                                        *
 *  The user can provide a callback function to redirect messages         *
 *  that would otherwise go to stderr.  Here are two examples:            *
 *  (1) to stop all messages:                                             *
 *      void send_to_devnull(const char *msg) {}                          *
 *  (2) to write to the system logger:                                    *
 *      void send_to_syslog(const char *msg) {                            *
 *           syslog(1, msg);                                              *
 *      }                                                                 *
 *  These would then be registered using                                  *
 *      leptSetStderrHandler(send_to_devnull);                            *
 *  and                                                                   *
 *      leptSetStderrHandler(send_to_syslog);                             *
 *------------------------------------------------------------------------*/
    /* By default, all messages go to stderr */
static void lept_default_stderr_handler(const char *formatted_msg)
{
    if (formatted_msg)
        fputs(formatted_msg, stderr);
}

    /* The stderr callback handler is private to leptonica.
     * By default it writes to stderr.  */
void (*stderr_handler)(const char *) = lept_default_stderr_handler;


/*!
 * \brief   leptSetStderrHandler()
 *
 * \param[in]    handler   callback function for lept_stderr output
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) This registers a handler for redirection of output to stderr
 *          at runtime.
 *      (2) If called with NULL, the output goes to stderr.
 * </pre>
 */
void leptSetStderrHandler(void (*handler)(const char *))
{
    if (handler)
        stderr_handler = handler;
    else
        stderr_handler = lept_default_stderr_handler;
}


#define MAX_DEBUG_MESSAGE   2000
/*!
 * \brief   lept_stderr()
 *
 * \param[in]    fmt      format string
 * \param[in]    ...      varargs
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) This is a replacement for fprintf(), to allow redirection
 *          of output.  All calls to fprintf(stderr, ...) are replaced
 *          with calls to lept_stderr(...).
 *      (2) The message size is limited to 2K bytes.
        (3) This utility was provided by jbarlow83.
 * </pre>
 */
void lept_stderr(const char *fmt, ...)
{
va_list  args;
char     msg[MAX_DEBUG_MESSAGE];
l_int32  n;

    va_start(args, fmt);
    n = vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    if (n < 0)
        return;
    (*stderr_handler)(msg);
}


/*--------------------------------------------------------------------*
 *                    Test files for equivalence                      *
 *--------------------------------------------------------------------*/
/*!
 * \brief   filesAreIdentical()
 *
 * \param[in]    fname1
 * \param[in]    fname2
 * \param[out]   psame     1 if identical; 0 if different
 * \return  0 if OK, 1 on error
 */
l_ok
filesAreIdentical(const char  *fname1,
                  const char  *fname2,
                  l_int32     *psame)
{
l_int32   i, same;
size_t    nbytes1, nbytes2;
l_uint8  *array1, *array2;

    if (!psame)
        return ERROR_INT("&same not defined", __func__, 1);
    *psame = 0;
    if (!fname1 || !fname2)
        return ERROR_INT("both names not defined", __func__, 1);

    nbytes1 = nbytesInFile(fname1);
    nbytes2 = nbytesInFile(fname2);
    if (nbytes1 != nbytes2)
        return 0;

    if ((array1 = l_binaryRead(fname1, &nbytes1)) == NULL)
        return ERROR_INT("array1 not read", __func__, 1);
    if ((array2 = l_binaryRead(fname2, &nbytes2)) == NULL) {
        LEPT_FREE(array1);
        return ERROR_INT("array2 not read", __func__, 1);
    }
    same = 1;
    for (i = 0; i < nbytes1; i++) {
        if (array1[i] != array2[i]) {
            same = 0;
            break;
        }
    }
    LEPT_FREE(array1);
    LEPT_FREE(array2);
    *psame = same;

    return 0;
}


/*--------------------------------------------------------------------------*
 *   16 and 32 bit byte-swapping on big endian and little  endian machines  *
 *--------------------------------------------------------------------------*
 *                                                                          *
 *   These are typically used for I/O conversions:                          *
 *      (1) endian conversion for data that was read from a file            *
 *      (2) endian conversion on data before it is written to a file        *
 *--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------*
 *                        16-bit byte swapping                        *
 *--------------------------------------------------------------------*/
#ifdef L_BIG_ENDIAN

l_uint16
convertOnBigEnd16(l_uint16  shortin)
{
    return ((shortin << 8) | (shortin >> 8));
}

l_uint16
convertOnLittleEnd16(l_uint16  shortin)
{
    return  shortin;
}

#else     /* L_LITTLE_ENDIAN */

l_uint16
convertOnLittleEnd16(l_uint16  shortin)
{
    return ((shortin << 8) | (shortin >> 8));
}

l_uint16
convertOnBigEnd16(l_uint16  shortin)
{
    return  shortin;
}

#endif  /* L_BIG_ENDIAN */


/*--------------------------------------------------------------------*
 *                        32-bit byte swapping                        *
 *--------------------------------------------------------------------*/
#ifdef L_BIG_ENDIAN

l_uint32
convertOnBigEnd32(l_uint32  wordin)
{
    return ((wordin << 24) | ((wordin << 8) & 0x00ff0000) |
            ((wordin >> 8) & 0x0000ff00) | (wordin >> 24));
}

l_uint32
convertOnLittleEnd32(l_uint32  wordin)
{
    return wordin;
}

#else  /*  L_LITTLE_ENDIAN */

l_uint32
convertOnLittleEnd32(l_uint32  wordin)
{
    return ((wordin << 24) | ((wordin << 8) & 0x00ff0000) |
            ((wordin >> 8) & 0x0000ff00) | (wordin >> 24));
}

l_uint32
convertOnBigEnd32(l_uint32  wordin)
{
    return wordin;
}

#endif  /* L_BIG_ENDIAN */


/*---------------------------------------------------------------------*
 *           File corruption and byte replacement operations           *
 *---------------------------------------------------------------------*/
/*!
 * \brief   fileCorruptByDeletion()
 *
 * \param[in]    filein
 * \param[in]    loc       fractional location of start of deletion
 * \param[in]    size      fractional size of deletion
 * \param[in]    fileout   corrupted file
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) %loc and %size are expressed as a fraction of the file size.
 *      (2) This makes a copy of the data in %filein, where bytes in the
 *          specified region have deleted.
 *      (3) If (%loc + %size) >= 1.0, this deletes from the position
 *          represented by %loc to the end of the file.
 *      (4) It is useful for testing robustness of I/O wrappers when the
 *          data is corrupted, by simulating data corruption by deletion.
 * </pre>
 */
l_ok
fileCorruptByDeletion(const char  *filein,
                      l_float32    loc,
                      l_float32    size,
                      const char  *fileout)
{
l_int32   i, locb, sizeb, rembytes;
size_t    inbytes, outbytes;
l_uint8  *datain, *dataout;

    if (!filein || !fileout)
        return ERROR_INT("filein and fileout not both specified", __func__, 1);
    if (loc < 0.0 || loc >= 1.0)
        return ERROR_INT("loc must be in [0.0 ... 1.0)", __func__, 1);
    if (size <= 0.0)
        return ERROR_INT("size must be > 0.0", __func__, 1);
    if (loc + size > 1.0)
        size = 1.0f - loc;

    datain = l_binaryRead(filein, &inbytes);
    locb = (l_int32)(loc * inbytes + 0.5);
    locb = L_MIN(locb, inbytes - 1);
    sizeb = (l_int32)(size * inbytes + 0.5);
    sizeb = L_MAX(1, sizeb);
    sizeb = L_MIN(sizeb, inbytes - locb);  /* >= 1 */
    L_INFO("Removed %d bytes at location %d\n", __func__, sizeb, locb);
    rembytes = inbytes - locb - sizeb;  /* >= 0; to be copied, after excision */

    outbytes = inbytes - sizeb;
    dataout = (l_uint8 *)LEPT_CALLOC(outbytes, 1);
    for (i = 0; i < locb; i++)
        dataout[i] = datain[i];
    for (i = 0; i < rembytes; i++)
        dataout[locb + i] = datain[locb + sizeb + i];
    l_binaryWrite(fileout, "w", dataout, outbytes);

    LEPT_FREE(datain);
    LEPT_FREE(dataout);
    return 0;
}


/*!
 * \brief   fileCorruptByMutation()
 *
 * \param[in]    filein
 * \param[in]    loc       fractional location of start of randomization
 * \param[in]    size      fractional size of randomization
 * \param[in]    fileout   corrupted file
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) %loc and %size are expressed as a fraction of the file size.
 *      (2) This makes a copy of the data in %filein, where bytes in the
 *          specified region have been replaced by random data.
 *      (3) If (%loc + %size) >= 1.0, this modifies data from the position
 *          represented by %loc to the end of the file.
 *      (4) It is useful for testing robustness of I/O wrappers when the
 *          data is corrupted, by simulating data corruption.
 * </pre>
 */
l_ok
fileCorruptByMutation(const char  *filein,
                      l_float32    loc,
                      l_float32    size,
                      const char  *fileout)
{
l_int32   i, locb, sizeb;
size_t    bytes;
l_uint8  *data;

    if (!filein || !fileout)
        return ERROR_INT("filein and fileout not both specified", __func__, 1);
    if (loc < 0.0 || loc >= 1.0)
        return ERROR_INT("loc must be in [0.0 ... 1.0)", __func__, 1);
    if (size <= 0.0)
        return ERROR_INT("size must be > 0.0", __func__, 1);
    if (loc + size > 1.0)
        size = 1.0f - loc;

    data = l_binaryRead(filein, &bytes);
    locb = (l_int32)(loc * bytes + 0.5);
    locb = L_MIN(locb, bytes - 1);
    sizeb = (l_int32)(size * bytes + 0.5);
    sizeb = L_MAX(1, sizeb);
    sizeb = L_MIN(sizeb, bytes - locb);  /* >= 1 */
    L_INFO("Randomizing %d bytes at location %d\n", __func__, sizeb, locb);

        /* Make an array of random bytes and do the substitution */
    for (i = 0; i < sizeb; i++) {
        data[locb + i] =
            (l_uint8)(255.9 * ((l_float64)rand() / (l_float64)RAND_MAX));
    }

    l_binaryWrite(fileout, "w", data, bytes);
    LEPT_FREE(data);
    return 0;
}


/*!
 * \brief   fileReplaceBytes()
 *
 * \param[in]    filein      input file
 * \param[in]    start       start location for replacement
 * \param[in]    nbytes      number of bytes to be removed
 * \param[in]    newdata     replacement bytes
 * \param[in]    newsize     size of replacement bytes
 * \param[in]    fileout     output file
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) To remove %nbytes without replacement, set %newdata == NULL.
 *      (2) One use is for replacing the date/time in a pdf file by a
 *          string of 12 '0's, effectively removing the date without
 *          invalidating the byte counters in the pdf file:
 *              fileReplaceBytes(filein 86 12 (char *)"000000000000" 12 fileout
 * </pre>
 */
l_ok
fileReplaceBytes(const char  *filein,
                 l_int32      start,
                 l_int32      nbytes,
                 l_uint8     *newdata,
                 size_t       newsize,
                 const char  *fileout)
{
l_int32   i, index;
size_t    inbytes, outbytes;
l_uint8  *datain, *dataout;

    if (!filein || !fileout)
        return ERROR_INT("filein and fileout not both specified", __func__, 1);

    datain = l_binaryRead(filein, &inbytes);
    if (start + nbytes > inbytes)
        L_WARNING("start + nbytes > length(filein) = %zu\n", __func__, inbytes);

    if (!newdata) newsize = 0;
    outbytes = inbytes - nbytes + newsize;
    if ((dataout = (l_uint8 *)LEPT_CALLOC(outbytes, 1)) == NULL) {
        LEPT_FREE(datain);
        return ERROR_INT("calloc fail for dataout", __func__, 1);
    }

    for (i = 0; i < start; i++)
        dataout[i] = datain[i];
    for (i = start; i < start + newsize; i++)
        dataout[i] = newdata[i - start];
    index = start + nbytes;  /* for datain */
    start += newsize;  /* for dataout */
    for (i = start; i < outbytes; i++, index++)
        dataout[i] = datain[index];
    l_binaryWrite(fileout, "w", dataout, outbytes);

    LEPT_FREE(datain);
    LEPT_FREE(dataout);
    return 0;
}


/*---------------------------------------------------------------------*
 *              Generate random integer in given interval              *
 *---------------------------------------------------------------------*/
/*!
 * \brief   genRandomIntOnInterval()
 *
 * \param[in]    start     beginning of interval; can be < 0
 * \param[in]    end       end of interval; must be >= start
 * \param[in]    seed      use 0 to skip; otherwise call srand
 * \param[out]   pval      random integer in interval [start ... end]
 * \return  0 if OK, 1 on error
 */
l_ok
genRandomIntOnInterval(l_int32   start,
                       l_int32   end,
                       l_int32   seed,
                       l_int32  *pval)
{
l_float64  range;

    if (!pval)
        return ERROR_INT("&val not defined", __func__, 1);
    *pval = 0;
    if (end < start)
        return ERROR_INT("invalid range", __func__, 1);

    if (seed > 0) srand(seed);
    range = (l_float64)(end - start + 1);
    *pval = start + (l_int32)((l_float64)range *
                       ((l_float64)rand() / (l_float64)RAND_MAX));
    return 0;
}


/*---------------------------------------------------------------------*
 *                        Simple math functions                        *
 *---------------------------------------------------------------------*/
/*!
 * \brief   lept_roundftoi()
 *
 * \param[in]    fval
 * \return  value rounded to the nearest integer
 *
 * <pre>
 * Notes:
 *      (1) For fval >= 0, fval --> round(fval) == floor(fval + 0.5)
 *          For fval < 0, fval --> -round(-fval))
 *          This is symmetric around 0.
 *          e.g., for fval in (-0.5 ... 0.5), fval --> 0
 * </pre>
 */
l_int32
lept_roundftoi(l_float32  fval)
{
    return (fval >= 0.0) ? (l_int32)(fval + 0.5) : (l_int32)(fval - 0.5);
}


/*!
 * \brief   lept_floor()
 *
 * \param[in]    fval
 * \return  largest integer that is not greater than %fval
 */
l_int32
lept_floor(l_float32  fval)
{
    return (fval >= 0.0) ? (l_int32)(fval) : -(l_int32)(-fval);
}


/*!
 * \brief   lept_ceiling()
 *
 * \param[in]    fval
 * \return  smallest integer that is not less than %fval
 *
 * <pre>
 * Notes:
 *      (1) If fval is equal to its interger value, return that.
 *          Otherwise:
 *            For fval > 0, fval --> 1 + floor(fval)
 *            For fval < 0, fval --> -(1 + floor(-fval))
 * </pre>
 */
l_int32
lept_ceiling(l_float32  fval)
{
    return (fval == (l_int32)fval ? (l_int32)fval :
            fval > 0.0 ? 1 + (l_int32)(fval) : -(1 + (l_int32)(-fval)));
}


/*---------------------------------------------------------------------*
 *                        64-bit hash functions                        *
 *---------------------------------------------------------------------*/
/*!
 * \brief   l_hashStringToUint64()
 *
 * \param[in]    str
 * \param[out]   phash    hash value
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The intent of the hash is to avoid collisions by mapping
 *          the string as randomly as possible into 64 bits.
 *      (2) To the extent that the hashes are random, the probability of
 *          a collision can be approximated by the square of the number
 *          of strings divided by 2^64.  For 1 million strings, the
 *          collision probability is about 1 in 16 million.
 *      (3) I expect non-randomness of the distribution to be most evident
 *          for small text strings.  This hash function has been tested
 *          for all 5-character text strings composed of 26 letters,
 *          of which there are 26^5 = 12356630.  There are no hash
 *          collisions for this set.
 * </pre>
 */
l_ok
l_hashStringToUint64(const char  *str,
                     l_uint64    *phash)
{
l_uint64  hash, mulp;

    if (phash) *phash = 0;
    if (!str || (str[0] == '\0'))
        return ERROR_INT("str not defined or empty", __func__, 1);
    if (!phash)
        return ERROR_INT("&hash not defined", __func__, 1);

    mulp = 26544357894361247;  /* prime, about 1/700 of the max uint64 */
    hash = 104395301;
    while (*str) {
        hash += (*str++ * mulp) ^ (hash >> 7);   /* shift [1...23] are ok */
    }
    *phash = hash ^ (hash << 37);
    return 0;
}


/*!
 * \brief   l_hashStringToUint64Fast()
 *
 * \param[in]    str
 * \param[out]   phash    hash value
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *     (1) This very simple hash algorithm is described in "The Practice
 *         of Programming" by Kernighan and Pike, p. 57 (1999).
 *     (2) The returned hash value would then be hashed into an index into
 *         the hashtable, using the mod operator with the hashtable size.
 * </pre>
 */
l_ok
l_hashStringToUint64Fast(const char  *str,
                         l_uint64    *phash)
{
l_uint64  h;
l_uint8  *p;

    if (phash) *phash = 0;
    if (!str || (str[0] == '\0'))
        return ERROR_INT("str not defined or empty", __func__, 1);
    if (!phash)
        return ERROR_INT("&hash not defined", __func__, 1);

    h = 0;
    for (p = (l_uint8 *)str; *p != '\0'; p++)
        h = 37 * h + *p;  /* 37 is good prime number for this */
    *phash = h;
    return 0;
}


/*!
 * \brief   l_hashPtToUint64()
 *
 * \param[in]    x, y
 * \param[out]   phash    hash value
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This simple hash function has no collisions for
 *          any of 400 million points with x and y up to 20000.
 * </pre>
 */
l_ok
l_hashPtToUint64(l_int32    x,
                 l_int32    y,
                 l_uint64  *phash)
{
    if (!phash)
        return ERROR_INT("&hash not defined", __func__, 1);

    *phash = (l_uint64)(2173249142.3849 * x + 3763193258.6227 * y);
    return 0;
}


/*!
 * \brief   l_hashFloat64ToUint64()
 *
 * \param[in]    val
 * \param[out]   phash      hash key
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is a simple hash for using hashmaps with 64-bit float data.
 *      (2) The resulting hash is called a "key" in a lookup operation.
 *          The bucket for %val in a hashmap is then found by taking the mod
 *          of the hash key with the number of buckets (which is prime).
 * </pre>
 */
l_ok
l_hashFloat64ToUint64(l_float64  val,
                      l_uint64  *phash)
{
    if (!phash)
        return ERROR_INT("&hash not defined", __func__, 1);
    val = (val >= 0.0) ? 847019.66701 * val : -217324.91613 * val;
    *phash = (l_uint64)val;
    return 0;
}


/*---------------------------------------------------------------------*
 *                           Prime finders                             *
 *---------------------------------------------------------------------*/
/*!
 * \brief   findNextLargerPrime()
 *
 * \param[in]    start
 * \param[out]   pprime    first prime larger than %start
 * \return  0 if OK, 1 on error
 */
l_ok
findNextLargerPrime(l_int32    start,
                    l_uint32  *pprime)
{
l_int32  i, is_prime;

    if (!pprime)
        return ERROR_INT("&prime not defined", __func__, 1);
    *pprime = 0;
    if (start <= 0)
        return ERROR_INT("start must be > 0", __func__, 1);

    for (i = start + 1; ; i++) {
        lept_isPrime(i, &is_prime, NULL);
        if (is_prime) {
            *pprime = i;
            return 0;
        }
    }

    return ERROR_INT("prime not found!", __func__, 1);
}


/*!
 * \brief   lept_isPrime()
 *
 * \param[in]    n           64-bit unsigned
 * \param[out]   pis_prime   1 if prime, 0 otherwise
 * \param[out]   pfactor     [optional] smallest divisor, or 0 on error
 *                           or if prime
 * \return  0 if OK, 1 on error
 */
l_ok
lept_isPrime(l_uint64   n,
             l_int32   *pis_prime,
             l_uint32  *pfactor)
{
l_uint32  div;
l_uint64  limit, ratio;

    if (pis_prime) *pis_prime = 0;
    if (pfactor) *pfactor = 0;
    if (!pis_prime)
        return ERROR_INT("&is_prime not defined", __func__, 1);
    if (n <= 0)
        return ERROR_INT("n must be > 0", __func__, 1);

    if (n % 2 == 0) {
        if (pfactor) *pfactor = 2;
        return 0;
    }

    limit = (l_uint64)sqrt((l_float64)n);
    for (div = 3; div < limit; div += 2) {
       ratio = n / div;
       if (ratio * div == n) {
           if (pfactor) *pfactor = div;
           return 0;
       }
    }

    *pis_prime = 1;
    return 0;
}


/*---------------------------------------------------------------------*
 *                         Gray code conversion                        *
 *---------------------------------------------------------------------*/
/*!
 * \brief   convertIntToGrayCode()
 *
 * \param[in]  val    integer value
 * \return     corresponding gray code value
 *
 * <pre>
 * Notes:
 *      (1) Gray code values corresponding to integers differ by
 *          only one bit transition between successive integers.
 * </pre>
 */
l_uint32
convertIntToGrayCode(l_uint32 val)
{
    return (val >> 1) ^ val;
}


/*!
 * \brief   convertGrayCodeToInt()
 *
 * \param[in]  val    gray code value
 * \return     corresponding integer value
 */
l_uint32
convertGrayCodeToInt(l_uint32 val)
{
l_uint32  shift;

    for (shift = 1; shift < 32; shift <<= 1)
        val ^= val >> shift;
    return val;
}


/*---------------------------------------------------------------------*
 *                       Leptonica version number                      *
 *---------------------------------------------------------------------*/
/*!
 * \brief   getLeptonicaVersion()
 *
 *      Return: string of version number (e.g., 'leptonica-1.74.2')
 *
 *  Notes:
 *      (1) The caller has responsibility to free the memory.
 */
char *
getLeptonicaVersion(void)
{
size_t  bufsize = 100;

    char *version = (char *)LEPT_CALLOC(bufsize, sizeof(char));

#ifdef _MSC_VER
  #ifdef _USRDLL
    char dllStr[] = "DLL";
  #else
    char dllStr[] = "LIB";
  #endif
  #ifdef _DEBUG
    char debugStr[] = "Debug";
  #else
    char debugStr[] = "Release";
  #endif
  #ifdef _M_IX86
    char bitStr[] = " x86";
  #elif _M_X64
    char bitStr[] = " x64";
  #else
    char bitStr[] = "";
  #endif
    snprintf(version, bufsize, "leptonica-%d.%d.%d (%s, %s) [MSC v.%d %s %s%s]",
           LIBLEPT_MAJOR_VERSION, LIBLEPT_MINOR_VERSION, LIBLEPT_PATCH_VERSION,
           __DATE__, __TIME__, _MSC_VER, dllStr, debugStr, bitStr);

#else

    snprintf(version, bufsize, "leptonica-%d.%d.%d", LIBLEPT_MAJOR_VERSION,
             LIBLEPT_MINOR_VERSION, LIBLEPT_PATCH_VERSION);

#endif   /* _MSC_VER */
    return version;
}


/*---------------------------------------------------------------------*
 *                           Timing procs                              *
 *---------------------------------------------------------------------*/
#if !defined(_WIN32) && !defined(__Fuchsia__)

#include <sys/time.h>
#include <sys/resource.h>

static struct rusage rusage_before;
static struct rusage rusage_after;

/*!
 * \brief   startTimer(), stopTimer()
 *
 *  Notes:
 *      (1) These measure the cpu time elapsed between the two calls:
 *            startTimer();
 *            ....
 *            lept_stderr( "Elapsed time = %7.3f sec\n", stopTimer());
 */
void
startTimer(void)
{
    getrusage(RUSAGE_SELF, &rusage_before);
}

l_float32
stopTimer(void)
{
l_int32  tsec, tusec;

    getrusage(RUSAGE_SELF, &rusage_after);

    tsec = rusage_after.ru_utime.tv_sec - rusage_before.ru_utime.tv_sec;
    tusec = rusage_after.ru_utime.tv_usec - rusage_before.ru_utime.tv_usec;
    return (tsec + ((l_float32)tusec) / 1000000.0);
}


/*!
 * \brief   startTimerNested(), stopTimerNested()
 *
 *  Example of usage:
 *
 *      L_TIMER  t1 = startTimerNested();
 *      ....
 *      L_TIMER  t2 = startTimerNested();
 *      ....
 *      lept_stderr( "Elapsed time 2 = %7.3f sec\n", stopTimerNested(t2));
 *      ....
 *      lept_stderr( "Elapsed time 1 = %7.3f sec\n", stopTimerNested(t1));
 */
L_TIMER
startTimerNested(void)
{
struct rusage  *rusage_start;

    rusage_start = (struct rusage *)LEPT_CALLOC(1, sizeof(struct rusage));
    getrusage(RUSAGE_SELF, rusage_start);
    return rusage_start;
}

l_float32
stopTimerNested(L_TIMER  rusage_start)
{
l_int32        tsec, tusec;
struct rusage  rusage_stop;

    getrusage(RUSAGE_SELF, &rusage_stop);

    tsec = rusage_stop.ru_utime.tv_sec -
           ((struct rusage *)rusage_start)->ru_utime.tv_sec;
    tusec = rusage_stop.ru_utime.tv_usec -
           ((struct rusage *)rusage_start)->ru_utime.tv_usec;
    LEPT_FREE(rusage_start);
    return (tsec + ((l_float32)tusec) / 1000000.0);
}


/*!
 * \brief   l_getCurrentTime()
 *
 * \param[out]   sec     [optional] in seconds since birth of Unix
 * \param[out]   usec    [optional] in microseconds since birth of Unix
 * \return  void
 */
void
l_getCurrentTime(l_int32  *sec,
                 l_int32  *usec)
{
struct timeval tv;

    gettimeofday(&tv, NULL);
    if (sec) *sec = (l_int32)tv.tv_sec;
    if (usec) *usec = (l_int32)tv.tv_usec;
}

#elif defined(__Fuchsia__) /* resource.h not implemented on Fuchsia. */

    /* Timer functions are used for testing and debugging, and
     * are stubbed out.  If they are needed in the future, they
     * can be implemented in Fuchsia using the zircon syscall
     * zx_object_get_info() in ZX_INFOR_THREAD_STATS mode.  */
void
startTimer(void)
{
}

l_float32
stopTimer(void)
{
    return 0.0;
}

L_TIMER
startTimerNested(void)
{
    return NULL;
}

l_float32
stopTimerNested(L_TIMER  rusage_start)
{
    return 0.0;
}

void
l_getCurrentTime(l_int32  *sec,
                 l_int32  *usec)
{
}

#else   /* _WIN32 : resource.h not implemented under Windows */

    /* Note: if division by 10^7 seems strange, the time is expressed
     * as the number of 100-nanosecond intervals that have elapsed
     * since 12:00 A.M. January 1, 1601.  */

static ULARGE_INTEGER utime_before;
static ULARGE_INTEGER utime_after;

void
startTimer(void)
{
HANDLE    this_process;
FILETIME  start, stop, kernel, user;

    this_process = GetCurrentProcess();

    GetProcessTimes(this_process, &start, &stop, &kernel, &user);

    utime_before.LowPart  = user.dwLowDateTime;
    utime_before.HighPart = user.dwHighDateTime;
}

l_float32
stopTimer(void)
{
HANDLE     this_process;
FILETIME   start, stop, kernel, user;
ULONGLONG  hnsec;  /* in units of hecto-nanosecond (100 ns) intervals */

    this_process = GetCurrentProcess();

    GetProcessTimes(this_process, &start, &stop, &kernel, &user);

    utime_after.LowPart  = user.dwLowDateTime;
    utime_after.HighPart = user.dwHighDateTime;
    hnsec = utime_after.QuadPart - utime_before.QuadPart;
    return (l_float32)(signed)hnsec / 10000000.0f;
}

L_TIMER
startTimerNested(void)
{
HANDLE           this_process;
FILETIME         start, stop, kernel, user;
ULARGE_INTEGER  *utime_start;

    this_process = GetCurrentProcess();

    GetProcessTimes (this_process, &start, &stop, &kernel, &user);

    utime_start = (ULARGE_INTEGER *)LEPT_CALLOC(1, sizeof(ULARGE_INTEGER));
    utime_start->LowPart  = user.dwLowDateTime;
    utime_start->HighPart = user.dwHighDateTime;
    return utime_start;
}

l_float32
stopTimerNested(L_TIMER  utime_start)
{
HANDLE          this_process;
FILETIME        start, stop, kernel, user;
ULARGE_INTEGER  utime_stop;
ULONGLONG       hnsec;  /* in units of 100 ns intervals */

    this_process = GetCurrentProcess ();

    GetProcessTimes (this_process, &start, &stop, &kernel, &user);

    utime_stop.LowPart  = user.dwLowDateTime;
    utime_stop.HighPart = user.dwHighDateTime;
    hnsec = utime_stop.QuadPart - ((ULARGE_INTEGER *)utime_start)->QuadPart;
    LEPT_FREE(utime_start);
    return (l_float32)(signed)hnsec / 10000000.0f;
}

void
l_getCurrentTime(l_int32  *sec,
                 l_int32  *usec)
{
ULARGE_INTEGER  utime, birthunix;
FILETIME        systemtime;
LONGLONG        birthunixhnsec = 116444736000000000;  /*in units of 100 ns */
LONGLONG        usecs;

    GetSystemTimeAsFileTime(&systemtime);
    utime.LowPart  = systemtime.dwLowDateTime;
    utime.HighPart = systemtime.dwHighDateTime;

    birthunix.LowPart = (DWORD) birthunixhnsec;
    birthunix.HighPart = birthunixhnsec >> 32;

    usecs = (LONGLONG) ((utime.QuadPart - birthunix.QuadPart) / 10);

    if (sec) *sec = (l_int32) (usecs / 1000000);
    if (usec) *usec = (l_int32) (usecs % 1000000);
}

#endif


/*!
 * \brief   startWallTimer()
 *
 * \return  walltimer-ptr
 *
 * <pre>
 * Notes:
 *      (1) These measure the wall clock time  elapsed between the two calls:
 *            L_WALLTIMER *timer = startWallTimer();
 *            ....
 *            lept_stderr( "Elapsed time = %f sec\n", stopWallTimer(&timer);
 *      (2) Note that the timer object is destroyed by stopWallTimer().
 * </pre>
 */
L_WALLTIMER *
startWallTimer(void)
{
L_WALLTIMER  *timer;

    timer = (L_WALLTIMER *)LEPT_CALLOC(1, sizeof(L_WALLTIMER));
    l_getCurrentTime(&timer->start_sec, &timer->start_usec);
    return timer;
}

/*!
 * \brief   stopWallTimer()
 *
 * \param[in,out]  ptimer     walltimer pointer
 * \return  time wall time elapsed in seconds
 */
l_float32
stopWallTimer(L_WALLTIMER  **ptimer)
{
l_int32       tsec, tusec;
L_WALLTIMER  *timer;

    if (!ptimer)
        return (l_float32)ERROR_FLOAT("&timer not defined", __func__, 0.0);
    timer = *ptimer;
    if (!timer)
        return (l_float32)ERROR_FLOAT("timer not defined", __func__, 0.0);

    l_getCurrentTime(&timer->stop_sec, &timer->stop_usec);
    tsec = timer->stop_sec - timer->start_sec;
    tusec = timer->stop_usec - timer->start_usec;
    LEPT_FREE(timer);
    *ptimer = NULL;
    return (tsec + ((l_float32)tusec) / 1000000.0f);
}


/*!
 * \brief   l_getFormattedDate()
 *
 * \return  formatted date string, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is used in pdf, in the form specified in section 3.8.2 of
 *          http://partners.adobe.com/public/developer/en/pdf/PDFReference.pdf
 *      (2) Contributed by Dave Bryan.  Works on all platforms.
 * </pre>
 */
char *
l_getFormattedDate(void)
{
char        buf[128] = "", sep = 'Z';
l_int32     gmt_offset, relh, relm;
time_t      ut, lt;
struct tm   Tm;
struct tm  *tptr = &Tm;

    ut = time(NULL);

        /* This generates a second "time_t" value by calling "gmtime" to
           fill in a "tm" structure expressed as UTC and then calling
           "mktime", which expects a "tm" structure expressed as the
           local time.  The result is a value that is offset from the
           value returned by the "time" function by the local UTC offset.
           "tm_isdst" is set to -1 to tell "mktime" to determine for
           itself whether DST is in effect.  This is necessary because
           "gmtime" always sets "tm_isdst" to 0, which would tell
           "mktime" to presume that DST is not in effect. */
#ifdef _WIN32
  #ifdef _MSC_VER
    gmtime_s(tptr, &ut);
  #else  /* mingw */
    tptr = gmtime(&ut);
  #endif
#else
    gmtime_r(&ut, tptr);
#endif
    tptr->tm_isdst = -1;
    lt = mktime(tptr);

        /* Calls "difftime" to obtain the resulting difference in seconds,
         * because "time_t" is an opaque type, per the C standard. */
    gmt_offset = (l_int32) difftime(ut, lt);
    if (gmt_offset > 0)
        sep = '+';
    else if (gmt_offset < 0)
        sep = '-';
    relh = L_ABS(gmt_offset) / 3600;
    relm = (L_ABS(gmt_offset) % 3600) / 60;

#ifdef _WIN32
  #ifdef _MSC_VER
    localtime_s(tptr, &ut);
  #else  /* mingw */
    tptr = localtime(&ut);
  #endif
#else
    localtime_r(&ut, tptr);
#endif
    strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", tptr);
    sprintf(buf + 14, "%c%02d'%02d'", sep, relh, relm);
    return stringNew(buf);
}
